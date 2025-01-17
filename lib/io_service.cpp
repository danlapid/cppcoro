///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/io_service.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/detail/async_operation.hpp>

#include <system_error>
#include <cassert>
#include <algorithm>
#include <thread>
#include <vector>

#if CPPCORO_OS_WINNT
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# ifndef NOMINMAX
#  define NOMINMAX
# endif
# include <windows.h>
#elif CPPCORO_OS_LINUX
# include <sys/timerfd.h>
#elif CPPCORO_OS_DARWIN
# include <sys/event.h>
#endif

#if CPPCORO_OS_WINNT
/// \brief
/// A queue of pending timers that supports efficiently determining
/// and dequeueing the earliest-due timers in the queue.
///
/// Implementation utilises a heap-sorted vector of entries with an
/// additional sorted linked-list that can be used as a fallback in
/// cases that there was insufficient memory to store all timer
/// entries in the vector.
///
/// This fallback is required to guarantee that all operations on this
/// queue are noexcept.s
class cppcoro::io_service::timer_queue
{
public:

	using time_point = std::chrono::high_resolution_clock::time_point;

	timer_queue() noexcept;

	~timer_queue();

	bool is_empty() const noexcept;

	time_point earliest_due_time() const noexcept;

	void enqueue_timer(cppcoro::io_service::timed_schedule_operation* timer) noexcept;

	void dequeue_due_timers(
		time_point currentTime,
		cppcoro::io_service::timed_schedule_operation*& timerList) noexcept;

	void remove_cancelled_timers(
		cppcoro::io_service::timed_schedule_operation*& timerList) noexcept;

private:

	struct timer_entry
	{
		timer_entry(cppcoro::io_service::timed_schedule_operation* timer)
			: m_dueTime(timer->m_resumeTime)
			, m_timer(timer)
		{}

		time_point m_dueTime;
		cppcoro::io_service::timed_schedule_operation* m_timer;
	};

	static bool compare_entries(const timer_entry& a, const timer_entry& b) noexcept
	{
		return a.m_dueTime > b.m_dueTime;
	}

	// A heap-sorted list of active timer entries
	// Earliest due timer is at the front of the queue
	std::vector<timer_entry> m_timerEntries;

	// Linked-list of overflow timer entries used in case there was
	// insufficient memory available to grow m_timerEntries.
	// List is sorted in ascending order of due-time using insertion-sort.
	// This is required to support the noexcept guarantee of enqueue_timer().
	cppcoro::io_service::timed_schedule_operation* m_overflowTimers;

};

cppcoro::io_service::timer_queue::timer_queue() noexcept
	: m_timerEntries()
	, m_overflowTimers(nullptr)
{}

cppcoro::io_service::timer_queue::~timer_queue()
{
	assert(is_empty());
}

bool cppcoro::io_service::timer_queue::is_empty() const noexcept
{
	return m_timerEntries.empty() && m_overflowTimers == nullptr;
}

cppcoro::io_service::timer_queue::time_point
cppcoro::io_service::timer_queue::earliest_due_time() const noexcept
{
	if (!m_timerEntries.empty())
	{
		if (m_overflowTimers != nullptr)
		{
			return std::min(
				m_timerEntries.front().m_dueTime,
				m_overflowTimers->m_resumeTime);
		}

		return m_timerEntries.front().m_dueTime;
	}
	else if (m_overflowTimers != nullptr)
	{
		return m_overflowTimers->m_resumeTime;
	}

	return time_point::max();
}

void cppcoro::io_service::timer_queue::enqueue_timer(
	cppcoro::io_service::timed_schedule_operation* timer) noexcept
{
	try
	{
		m_timerEntries.emplace_back(timer);
		std::push_heap(m_timerEntries.begin(), m_timerEntries.end(), compare_entries);
	}
	catch (...)
	{
		const auto& newDueTime = timer->m_resumeTime;

		auto** current = &m_overflowTimers;
		while ((*current) != nullptr && (*current)->m_resumeTime <= newDueTime)
		{
			current = &(*current)->m_next;
		}

		timer->m_next = *current;
		*current = timer;
	}
}

void cppcoro::io_service::timer_queue::dequeue_due_timers(
	time_point currentTime,
	cppcoro::io_service::timed_schedule_operation*& timerList) noexcept
{
	while (!m_timerEntries.empty() && m_timerEntries.front().m_dueTime <= currentTime)
	{
		auto* timer = m_timerEntries.front().m_timer;
		std::pop_heap(m_timerEntries.begin(), m_timerEntries.end(), compare_entries);
		m_timerEntries.pop_back();

		timer->m_next = timerList;
		timerList = timer;
	}

	while (m_overflowTimers != nullptr && m_overflowTimers->m_resumeTime <= currentTime)
	{
		auto* timer = m_overflowTimers;
		m_overflowTimers = timer->m_next;
		timer->m_next = timerList;
		timerList = timer;
	}
}

void cppcoro::io_service::timer_queue::remove_cancelled_timers(
	cppcoro::io_service::timed_schedule_operation*& timerList) noexcept
{
	// Perform a linear scan of all timers looking for any that have
	// had cancellation requested.

	const auto addTimerToList = [&](timed_schedule_operation* timer)
	{
		timer->m_next = timerList;
		timerList = timer;
	};

	const auto isTimerCancelled = [](const timer_entry& entry)
	{
		return entry.m_timer->m_cancellationToken.is_cancellation_requested();
	};

	auto firstCancelledEntry = std::find_if(
		m_timerEntries.begin(),
		m_timerEntries.end(),
		isTimerCancelled);
	if (firstCancelledEntry != m_timerEntries.end())
	{
		auto nonCancelledEnd = firstCancelledEntry;
		addTimerToList(nonCancelledEnd->m_timer);

		for (auto iter = firstCancelledEntry + 1;
			iter != m_timerEntries.end();
			++iter)
		{
			if (isTimerCancelled(*iter))
			{
				addTimerToList(iter->m_timer);
			}
			else
			{
				*nonCancelledEnd++ = std::move(*iter);
			}
		}

		m_timerEntries.erase(nonCancelledEnd, m_timerEntries.end());

		std::make_heap(
			m_timerEntries.begin(),
			m_timerEntries.end(),
			compare_entries);
	}

	{
		timed_schedule_operation** current = &m_overflowTimers;
		while ((*current) != nullptr)
		{
			auto* timer = (*current);
			if (timer->m_cancellationToken.is_cancellation_requested())
			{
				*current = timer->m_next;
				addTimerToList(timer);
			}
			else
			{
				current = &timer->m_next;
			}
		}
	}
}

class cppcoro::io_service::timer_thread_state
{
public:

	timer_thread_state();
	~timer_thread_state();

	timer_thread_state(const timer_thread_state& other) = delete;
	timer_thread_state& operator=(const timer_thread_state& other) = delete;

	void request_timer_cancellation() noexcept;

	void run() noexcept;

	void wake_up_timer_thread() noexcept;

	detail::win32::safe_handle m_wakeUpEvent;
	detail::win32::safe_handle m_waitableTimerEvent;

	std::atomic<io_service::timed_schedule_operation*> m_newlyQueuedTimers;
	std::atomic<bool> m_timerCancellationRequested;
	std::atomic<bool> m_shutDownRequested;

	std::thread m_thread;
};
#endif



cppcoro::io_service::io_service()
	: io_service(0)
{
}

cppcoro::io_service::io_service(std::uint32_t concurrencyHint)
	: m_threadState(0)
	, m_workCount(0)
	, m_mq(concurrencyHint)
	, m_scheduleOperations(nullptr)
#if CPPCORO_OS_WINNT
	, m_timerState(nullptr)
#endif
{
}

cppcoro::io_service::~io_service()
{
	assert(m_scheduleOperations.load(std::memory_order_relaxed) == nullptr);
	assert(m_threadState.load(std::memory_order_relaxed) < active_thread_count_increment);

#if CPPCORO_OS_WINNT
	delete m_timerState.load(std::memory_order_relaxed);
#endif
}

cppcoro::io_service::schedule_operation cppcoro::io_service::schedule() noexcept
{
	return schedule_operation{ *this };
}

std::uint64_t cppcoro::io_service::process_events()
{
	std::uint64_t eventCount = 0;
	if (try_enter_event_loop())
	{
		auto exitLoop = on_scope_exit([&] { exit_event_loop(); });

		constexpr bool waitForEvent = true;
		while (try_process_one_event(waitForEvent))
		{
			++eventCount;
		}
	}

	return eventCount;
}

std::uint64_t cppcoro::io_service::process_pending_events()
{
	std::uint64_t eventCount = 0;
	if (try_enter_event_loop())
	{
		auto exitLoop = on_scope_exit([&] { exit_event_loop(); });

		constexpr bool waitForEvent = false;
		while (try_process_one_event(waitForEvent))
		{
			++eventCount;
		}
	}

	return eventCount;
}

std::uint64_t cppcoro::io_service::process_one_event()
{
	std::uint64_t eventCount = 0;
	if (try_enter_event_loop())
	{
		auto exitLoop = on_scope_exit([&] { exit_event_loop(); });

		constexpr bool waitForEvent = true;
		if (try_process_one_event(waitForEvent))
		{
			++eventCount;
		}
	}

	return eventCount;
}

std::uint64_t cppcoro::io_service::process_one_pending_event()
{
	std::uint64_t eventCount = 0;
	if (try_enter_event_loop())
	{
		auto exitLoop = on_scope_exit([&] { exit_event_loop(); });

		constexpr bool waitForEvent = false;
		if (try_process_one_event(waitForEvent))
		{
			++eventCount;
		}
	}

	return eventCount;
}

void cppcoro::io_service::stop() noexcept
{
	const auto oldState = m_threadState.fetch_or(stop_requested_flag, std::memory_order_release);
	if ((oldState & stop_requested_flag) == 0)
	{
		for (auto activeThreadCount = oldState / active_thread_count_increment;
			activeThreadCount > 0;
			--activeThreadCount)
		{
			post_wake_up_event();
		}
	}
}

void cppcoro::io_service::reset()
{
	const auto oldState = m_threadState.fetch_and(~stop_requested_flag, std::memory_order_relaxed);

	// Check that there were no active threads running the event loop.
	assert(oldState == stop_requested_flag);
}

bool cppcoro::io_service::is_stop_requested() const noexcept
{
	return (m_threadState.load(std::memory_order_acquire) & stop_requested_flag) != 0;
}

void cppcoro::io_service::notify_work_started() noexcept
{
	m_workCount.fetch_add(1, std::memory_order_relaxed);
}

void cppcoro::io_service::notify_work_finished() noexcept
{
	if (m_workCount.fetch_sub(1, std::memory_order_relaxed) == 1)
	{
		stop();
	}
}

cppcoro::detail::message_queue& cppcoro::io_service::get_io_context() noexcept
{
	return m_mq;
}

void cppcoro::io_service::schedule_impl(schedule_operation* operation) noexcept
{
	const bool ok = m_mq.enqueue_message(
		{
			detail::message_type::RESUME_TYPE,
			reinterpret_cast<void*>(operation->m_awaiter.address()),
		});
	if (!ok)
	{
		// Failed to post to the I/O completion port.
		//
		// This is most-likely because the queue is currently full.
		//
		// We'll queue up the operation to a linked-list using a lock-free
		// push and defer the dispatch to the completion port until some I/O
		// thread next enters its event loop.
		auto* head = m_scheduleOperations.load(std::memory_order_acquire);
		do
		{
			operation->m_next = head;
		} while (!m_scheduleOperations.compare_exchange_weak(
			head,
			operation,
			std::memory_order_release,
			std::memory_order_acquire));
	}
}

void cppcoro::io_service::try_reschedule_overflow_operations() noexcept
{
	auto* operation = m_scheduleOperations.exchange(nullptr, std::memory_order_acquire);
	while (operation != nullptr)
	{
		auto* next = operation->m_next;
 		bool ok = m_mq.enqueue_message({
				detail::message_type::RESUME_TYPE,
				reinterpret_cast<void*>(operation->m_awaiter.address()),
			});
		if (!ok)
		{
			// Still unable to queue these operations.
			// Put them back on the list of overflow operations.
			auto* tail = operation;
			while (tail->m_next != nullptr)
			{
				tail = tail->m_next;
			}

			schedule_operation* head = nullptr;
			while (!m_scheduleOperations.compare_exchange_weak(
				head,
				operation,
				std::memory_order_release,
				std::memory_order_relaxed))
			{
				tail->m_next = head;
			}

			return;
		}

		operation = next;
	}
}

bool cppcoro::io_service::try_enter_event_loop() noexcept
{
	auto currentState = m_threadState.load(std::memory_order_relaxed);
	do
	{
		if ((currentState & stop_requested_flag) != 0)
		{
			return false;
		}
	} while (!m_threadState.compare_exchange_weak(
		currentState,
		currentState + active_thread_count_increment,
		std::memory_order_relaxed));

	return true;
}

void cppcoro::io_service::exit_event_loop() noexcept
{
	m_threadState.fetch_sub(active_thread_count_increment, std::memory_order_relaxed);
}

bool cppcoro::io_service::try_process_one_event(bool waitForEvent)
{
	if (is_stop_requested())
	{
		return false;
	}
 	while (true)
 	{
 		try_reschedule_overflow_operations();
		detail::message message;
 		bool ok = m_mq.dequeue_message(message, waitForEvent);

 		if (!ok)
 		{
 			return false;
 		}
		switch (message.type)
		{
		case detail::message_type::CALLBACK_TYPE:
		{
 			auto* state = static_cast<detail::async_operation_base*>(message.data);
 			state->m_callback(state);
 			return true;
		}
		case detail::message_type::RESUME_TYPE:
		{
			cppcoro::coroutine_handle<>::from_address(message.data).resume();
			return true;
		}
		case detail::message_type::WAKEUP_TYPE:
		{
 			if (is_stop_requested())
 			{
 				return false;
 			}
			break;
		}
		}
 	}
}

void cppcoro::io_service::post_wake_up_event() noexcept
{
	// We intentionally ignore the return code here.
	//
	// Assume that if posting an event failed that it failed because the queue was full
	// and the system is out of memory. In this case threads should find other events
	// in the queue next time they check anyway and thus wake-up.
 	(void)m_mq.enqueue_message({detail::message_type::WAKEUP_TYPE, NULL});
}

#if CPPCORO_OS_WINNT
cppcoro::io_service::timer_thread_state*
cppcoro::io_service::ensure_timer_thread_started()
{
	auto* timerState = m_timerState.load(std::memory_order_acquire);
	if (timerState == nullptr)
	{
		auto newTimerState = std::make_unique<timer_thread_state>();
		if (m_timerState.compare_exchange_strong(
			timerState,
			newTimerState.get(),
			std::memory_order_release,
			std::memory_order_acquire))
		{
			// We managed to install our timer_thread_state before some
			// other thread did, don't free it here - it will be freed in
			// the io_service destructor.
			timerState = newTimerState.release();
		}
	}

	return timerState;
}

cppcoro::io_service::timer_thread_state::timer_thread_state()
	: m_wakeUpEvent(detail::win32::create_auto_reset_event())
	, m_waitableTimerEvent(detail::win32::create_waitable_timer_event())
	, m_newlyQueuedTimers(nullptr)
	, m_timerCancellationRequested(false)
	, m_shutDownRequested(false)
	, m_thread([this] { this->run(); })
{
}

cppcoro::io_service::timer_thread_state::~timer_thread_state()
{
	m_shutDownRequested.store(true, std::memory_order_release);
	wake_up_timer_thread();
	m_thread.join();
}

void cppcoro::io_service::timer_thread_state::request_timer_cancellation() noexcept
{
	const bool wasTimerCancellationAlreadyRequested =
		m_timerCancellationRequested.exchange(true, std::memory_order_release);
	if (!wasTimerCancellationAlreadyRequested)
	{
		wake_up_timer_thread();
	}
}

void cppcoro::io_service::timer_thread_state::run() noexcept
{
	using clock = std::chrono::high_resolution_clock;
	using time_point = clock::time_point;

	timer_queue timerQueue;

	const DWORD waitHandleCount = 2;
	const HANDLE waitHandles[waitHandleCount] =
	{
		m_wakeUpEvent.handle(),
		m_waitableTimerEvent.handle()
	};

	time_point lastSetWaitEventTime = time_point::max();

	timed_schedule_operation* timersReadyToResume = nullptr;

	while (!m_shutDownRequested.load(std::memory_order_relaxed))
	{
		bool waitEvent = false;
 		bool timerEvent = false;
		DWORD timeout = INFINITE;
		const DWORD waitResult = ::WaitForMultipleObjectsEx(
			waitHandleCount,
			waitHandles,
			FALSE, // waitAll
			timeout,
			FALSE); // alertable
 		if (waitResult == WAIT_OBJECT_0 || waitResult == WAIT_FAILED)
 		{
 			waitEvent = true;
 		}
 		else if (waitResult == WAIT_OBJECT_0 + 1)
 		{
 			timerEvent = true;
 		}
 		if (waitEvent)
		{
			// Wake-up event (WAIT_OBJECT_0)
			//
			// We are only woken up for:
			// - handling timer cancellation
			// - handling newly queued timers
			// - shutdown
			//
			// We also handle WAIT_FAILED here so that we remain responsive
			// to new timers and cancellation even if the OS fails to perform
			// the wait operation for some reason.

			// Handle cancelled timers
			if (m_timerCancellationRequested.exchange(false, std::memory_order_acquire))
			{
				timerQueue.remove_cancelled_timers(timersReadyToResume);
			}

			// Handle newly queued timers
			auto* newTimers = m_newlyQueuedTimers.exchange(nullptr, std::memory_order_acquire);
			while (newTimers != nullptr)
			{
				auto* timer = newTimers;
				newTimers = timer->m_next;

				if (timer->m_cancellationToken.is_cancellation_requested())
				{
					timer->m_next = timersReadyToResume;
					timersReadyToResume = timer;
				}
				else
				{
					timerQueue.enqueue_timer(timer);
				}
			}
		}
		else if (timerEvent)
		{
			lastSetWaitEventTime = time_point::max();
		}

		if (!timerQueue.is_empty())
		{
			time_point currentTime = clock::now();

			timerQueue.dequeue_due_timers(currentTime, timersReadyToResume);

			if (!timerQueue.is_empty())
			{
				auto earliestDueTime = timerQueue.earliest_due_time();
				assert(earliestDueTime > currentTime);

				// Set the waitable timer before trying to schedule any of the ready-to-run
				// timers to avoid the concept of 'current time' on which we calculate the
				// amount of time to wait until the next timer is ready.
				if (earliestDueTime != lastSetWaitEventTime)
				{
					using ticks = std::chrono::duration<LONGLONG, std::ratio<1, 10'000'000>>;

					auto timeUntilNextDueTime = earliestDueTime - currentTime;

					// Negative value indicates relative time.
					LARGE_INTEGER dueTime;
					dueTime.QuadPart = -std::chrono::duration_cast<ticks>(timeUntilNextDueTime).count();

					// Period of 0 indicates no repeat on the timer.
					const LONG period = 0;

					// Don't wake the system from a suspended state just to
					// raise the timer event.
					const BOOL resumeFromSuspend = FALSE;

					const BOOL ok = ::SetWaitableTimer(
						m_waitableTimerEvent.handle(),
						&dueTime,
						period,
						nullptr,
						nullptr,
						resumeFromSuspend);
					if (ok)
					{
						lastSetWaitEventTime = earliestDueTime;
						timeout = INFINITE;
					}
					else
					{
						// Not sure what could cause the call to SetWaitableTimer()
						// to fail here but we'll just try falling back to using
						// the timeout parameter of the WaitForMultipleObjects() call.
						//
						// wake-up at least once every second and retry setting
						// the timer at that point.
						using namespace std::literals::chrono_literals;
						if (timeUntilNextDueTime > 1s)
						{
							timeout = 1000;
						}
						else if (timeUntilNextDueTime > 1ms)
						{
							timeout = static_cast<DWORD>(
								std::chrono::duration_cast<std::chrono::milliseconds>(
									timeUntilNextDueTime).count());
						}
						else
						{
							timeout = 1;
						}
					}
				}
			}
		}

		// Now schedule any ready-to-run timers.
		while (timersReadyToResume != nullptr)
		{
			auto* timer = timersReadyToResume;
			auto* nextTimer = timer->m_next;

			// Use 'release' memory order to ensure that any prior writes to
			// m_next "happen before" any potential uses of that same memory
			// back on the thread that is executing timed_schedule_operation::await_suspend()
			// which has the synchronising 'acquire' semantics.
			if (timer->m_refCount.fetch_sub(1, std::memory_order_release) == 1)
			{
				timer->m_scheduleOperation.m_service.schedule_impl(
					&timer->m_scheduleOperation);
			}

			timersReadyToResume = nextTimer;
		}
	}
}

void cppcoro::io_service::timer_thread_state::wake_up_timer_thread() noexcept
{
	(void)::SetEvent(m_wakeUpEvent.handle());
}

void cppcoro::io_service::schedule_operation::await_suspend(
	cppcoro::coroutine_handle<> awaiter) noexcept
{
	m_awaiter = awaiter;
	m_service.schedule_impl(this);
}

cppcoro::io_service::timed_schedule_operation::timed_schedule_operation(
	io_service& service,
	std::chrono::high_resolution_clock::time_point resumeTime,
	cppcoro::cancellation_token cancellationToken) noexcept
	: m_scheduleOperation(service)
	, m_resumeTime(resumeTime)
	, m_cancellationToken(std::move(cancellationToken))
	, m_refCount(2)
{
}

cppcoro::io_service::timed_schedule_operation::timed_schedule_operation(
	timed_schedule_operation&& other) noexcept
	: m_scheduleOperation(std::move(other.m_scheduleOperation))
	, m_resumeTime(std::move(other.m_resumeTime))
	, m_cancellationToken(std::move(other.m_cancellationToken))
	, m_refCount(2)
{
}

cppcoro::io_service::timed_schedule_operation::~timed_schedule_operation()
{
}

bool cppcoro::io_service::timed_schedule_operation::await_ready() const noexcept
{
	return m_cancellationToken.is_cancellation_requested();
}

void cppcoro::io_service::timed_schedule_operation::await_suspend(
	cppcoro::coroutine_handle<> awaiter)
{
	m_scheduleOperation.m_awaiter = awaiter;

	auto& service = m_scheduleOperation.m_service;

	// Ensure the timer state is initialised and the timer thread started.
	auto* timerState = service.ensure_timer_thread_started();

	if (m_cancellationToken.can_be_cancelled())
	{
		m_cancellationRegistration.emplace(m_cancellationToken, [timerState]
		{
			timerState->request_timer_cancellation();
		});
	}

	// Queue the timer schedule to the queue of incoming new timers.
	//
	// We need to do a careful dance here because it could be possible
	// that immediately after queueing the timer this thread could be
	// context-switched out, the timer thread could pick it up and
	// schedule it to be resumed, it could be resumed on an I/O thread
	// and complete its work and the io_service could be destructed.
	// All before we get to execute timerState.wake_up_timer_thread()
	// below. To work around this race we use a reference-counter
	// with initial value 2 and have both the timer thread and this
	// thread decrement the count once the awaiter is ready to be
	// rescheduled. Whichever thread decrements the ref-count to 0
	// is responsible for scheduling the awaiter for resumption.


	// Not sure if we need 'acquire' semantics on this load and
	// on the failure-case of the compare_exchange below.
	//
	// It could potentially be made 'release' if we can guarantee
	// that a read-with 'acquire' semantics in the timer thread
	// of the latest value will synchronise with all prior writes
	// to that value that used 'release' semantics.
	auto* prev = timerState->m_newlyQueuedTimers.load(std::memory_order_acquire);
	do
	{
		m_next = prev;
	} while (!timerState->m_newlyQueuedTimers.compare_exchange_weak(
		prev,
		this,
		std::memory_order_release,
		std::memory_order_acquire));

	if (prev == nullptr)
	{
		timerState->wake_up_timer_thread();
	}

	// Use 'acquire' semantics here to synchronise with the 'release'
	// operation performed on the timer thread to ensure that we have
	// seen all potential writes to this object. Without this, it's
	// possible that a write to the m_next field by the timer thread
	// will race with subsequent writes to that same memory by this
	// thread or whatever I/O thread resumes the coroutine.
	if (m_refCount.fetch_sub(1, std::memory_order_acquire) == 1)
	{
		service.schedule_impl(&m_scheduleOperation);
	}
}

void cppcoro::io_service::timed_schedule_operation::await_resume()
{
	m_cancellationRegistration.reset();
	m_cancellationToken.throw_if_cancellation_requested();
}

#elif CPPCORO_OS_LINUX
void cppcoro::io_service::schedule_operation::await_suspend(
	cppcoro::coroutine_handle<> awaiter) noexcept
{
	m_awaiter = awaiter;
	m_service.schedule_impl(this);
}

cppcoro::io_service::timed_schedule_operation::timed_schedule_operation(
	io_service& service,
	std::chrono::high_resolution_clock::time_point resumeTime,
	cppcoro::cancellation_token&& ct) noexcept
	: cppcoro::detail::async_operation_cancellable<timed_schedule_operation>(
 				&service, std::move(ct))
	, m_resumeTime(resumeTime)
 	, m_timerfd(detail::linux::create_timer_fd())
{}

bool cppcoro::io_service::timed_schedule_operation::try_start() noexcept {
	m_fd = m_timerfd.fd();
	std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
	auto waitTime = m_resumeTime - currentTime;
	itimerspec alarm_time = { 0 };
	if (waitTime.count() <= 0) {
		//linux timer of 0 time will not generate events
		//so let's set it to 1 nsec
		alarm_time.it_value.tv_nsec = 1;
	} else {
		auto seconds = std::chrono::duration_cast<std::chrono::seconds>(waitTime);
		auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(waitTime - seconds);
		alarm_time.it_value.tv_sec = seconds.count();
		alarm_time.it_value.tv_nsec = nanoseconds.count();
	}
	const bool ok = timerfd_settime(m_timerfd.fd(), 0, &alarm_time, NULL) != -1;
	if (!ok) {
		m_res = -errno;
		return false;
	}
	m_completeFunc = [&]() {
		return 0;
	};
	m_ioService->get_io_context().watch_handle(m_timerfd.fd(), reinterpret_cast<void*>(this), detail::watch_type::readable);
	return true;
}
#elif CPPCORO_OS_DARWIN
void cppcoro::io_service::schedule_operation::await_suspend(
	cppcoro::coroutine_handle<> awaiter) noexcept 
{
	m_awaiter = awaiter;
	m_service.schedule_impl(this);
}

cppcoro::io_service::timed_schedule_operation::timed_schedule_operation(
	io_service& service,
	std::chrono::high_resolution_clock::time_point resumeTime,
	cppcoro::cancellation_token&& ct) noexcept
	: cppcoro::detail::async_operation_cancellable<timed_schedule_operation>(
		  &service, std::move(ct))
	, m_resumeTime(resumeTime)
 	, m_timerfd(detail::darwin::create_timer_fd())
{
}

bool cppcoro::io_service::timed_schedule_operation::try_start() noexcept
{
	std::chrono::high_resolution_clock::time_point currentTime =
		std::chrono::high_resolution_clock::now();
	auto waitTime = m_resumeTime - currentTime;
	auto miliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(waitTime);
	struct kevent event;
	EV_SET(&event, m_timerfd.fd(), EVFILT_TIMER, EV_ADD | EV_ONESHOT, 0, miliseconds.count(), reinterpret_cast<void*>(this));
	m_completeFunc = [&]() { return 0; };
	m_ioService->get_io_context().watch_event(&event, reinterpret_cast<void*>(this));
	return true;
}
#endif
