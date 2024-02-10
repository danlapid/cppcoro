///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_IO_SERVICE_HPP_INCLUDED
#define CPPCORO_IO_SERVICE_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/cancellation_token.hpp>
#include <cppcoro/cancellation_registration.hpp>

#include <cppcoro/detail/platform.hpp>
#include <cppcoro/detail/async_operation.hpp>
#include <cppcoro/detail/message_queue.hpp>

#include <optional>
#include <chrono>
#include <cstdint>
#include <atomic>
#include <utility>
#include <mutex>
#include <cppcoro/coroutine.hpp>

namespace cppcoro
{
	class io_service
	{
	public:

		class schedule_operation;
		class timed_schedule_operation;

		/// Initialises the io_service.
		///
		/// Does not set a concurrency hint. All threads that enter the
		/// event loop will actively process events.
		io_service();

		/// Initialise the io_service with a concurrency hint.
		///
		/// \param concurrencyHint
		/// Specifies the target maximum number of I/O threads to be
		/// actively processing events.
		/// Note that the number of active threads may temporarily go
		/// above this number.
		io_service(std::uint32_t concurrencyHint);

		~io_service();

		io_service(io_service&& other) = delete;
		io_service(const io_service& other) = delete;
		io_service& operator=(io_service&& other) = delete;
		io_service& operator=(const io_service& other) = delete;

		/// Returns an operation that when awaited suspends the awaiting
		/// coroutine and reschedules it for resumption on an I/O thread
		/// associated with this io_service.
		[[nodiscard]]
		schedule_operation schedule() noexcept;

		/// Returns an operation that when awaited will suspend the
		/// awaiting coroutine for the specified delay. Once the delay
		/// has elapsed, the coroutine will resume execution on an
		/// I/O thread associated with this io_service.
		///
		/// \param delay
		/// The amount of time to delay scheduling resumption of the coroutine
		/// on an I/O thread. There is no guarantee that the coroutine will
		/// be resumed exactly after this delay.
		///
		/// \param cancellationToken [optional]
		/// A cancellation token that can be used to communicate a request to
		/// cancel the delayed schedule operation and schedule it for resumption
		/// immediately.
		/// The co_await operation will throw cppcoro::operation_cancelled if
		/// cancellation was requested before the coroutine could be resumed.
		template<typename REP, typename PERIOD>
		[[nodiscard]]
		timed_schedule_operation schedule_after(
			const std::chrono::duration<REP, PERIOD>& delay,
			cancellation_token cancellationToken = {}) noexcept;

		/// Process events until the io_service is stopped.
		///
		/// \return
		/// The number of events processed during this call.
		std::uint64_t process_events();

		/// Process events until either the io_service is stopped or
		/// there are no more pending events in the queue.
		///
		/// \return
		/// The number of events processed during this call.
		std::uint64_t process_pending_events();

		/// Block until either one event is processed or the io_service is stopped.
		///
		/// \return
		/// The number of events processed during this call.
		/// This will either be 0 or 1.
		std::uint64_t process_one_event();

		/// Process one event if there are any events pending, otherwise if there
		/// are no events pending or the io_service is stopped then return immediately.
		///
		/// \return
		/// The number of events processed during this call.
		/// This will either be 0 or 1.
		std::uint64_t process_one_pending_event();

		/// Shut down the io_service.
		///
		/// This will cause any threads currently in a call to one of the process_xxx() methods
		/// to return from that call once they finish processing the current event.
		///
		/// This call does not wait until all threads have exited the event loop so you
		/// must use other synchronisation mechanisms to wait for those threads.
		void stop() noexcept;

		/// Reset an io_service to prepare it for resuming processing of events.
		///
		/// Call this after a call to stop() to allow calls to process_xxx() methods
		/// to process events.
		///
		/// After calling stop() you should ensure that all threads have returned from
		/// calls to process_xxx() methods before calling reset().
		void reset();

		bool is_stop_requested() const noexcept;

		void notify_work_started() noexcept;

		void notify_work_finished() noexcept;

		detail::message_queue& get_io_context() noexcept;

	private:
#if CPPCORO_OS_WINNT
		class timer_thread_state;
		class timer_queue;
#endif

		friend class schedule_operation;
		friend class timed_schedule_operation;

		void schedule_impl(schedule_operation* operation) noexcept;

		void try_reschedule_overflow_operations() noexcept;

		bool try_enter_event_loop() noexcept;
		void exit_event_loop() noexcept;

		bool try_process_one_event(bool waitForEvent);

		void post_wake_up_event() noexcept;

#if CPPCORO_OS_WINNT
		timer_thread_state* ensure_timer_thread_started();
#endif

		static constexpr std::uint32_t stop_requested_flag = 1;
		static constexpr std::uint32_t active_thread_count_increment = 2;

		// Bit 0: stop_requested_flag
		// Bit 1-31: count of active threads currently running the event loop
		std::atomic<std::uint32_t> m_threadState;

		std::atomic<std::uint32_t> m_workCount;

 		detail::message_queue m_mq;

		// Head of a linked-list of schedule operations that are
		// ready to run but that failed to be queued to the I/O
		// completion port (eg. due to low memory).
		std::atomic<schedule_operation*> m_scheduleOperations;

#if CPPCORO_OS_WINNT
		std::atomic<timer_thread_state*> m_timerState;
#endif
	};

	class io_service::schedule_operation
	{
	public:

		schedule_operation(io_service& service) noexcept
			: m_service(service)
		{}

		bool await_ready() const noexcept { return false; }
		void await_suspend(cppcoro::coroutine_handle<> awaiter) noexcept;
		void await_resume() const noexcept {}

	private:

		friend class io_service;
		friend class io_service::timed_schedule_operation;

		io_service& m_service;
		cppcoro::coroutine_handle<> m_awaiter;
		schedule_operation* m_next;

	};

#if CPPCORO_OS_WINNT
	class io_service::timed_schedule_operation
	{
	public:

		timed_schedule_operation(
			io_service& service,
			std::chrono::high_resolution_clock::time_point resumeTime,
			cppcoro::cancellation_token cancellationToken) noexcept;

		timed_schedule_operation(timed_schedule_operation&& other) noexcept;

		~timed_schedule_operation();

		timed_schedule_operation& operator=(timed_schedule_operation&& other) = delete;
		timed_schedule_operation(const timed_schedule_operation& other) = delete;
		timed_schedule_operation& operator=(const timed_schedule_operation& other) = delete;

		bool await_ready() const noexcept;
		void await_suspend(cppcoro::coroutine_handle<> awaiter);
		void await_resume();

	private:

		friend class io_service::timer_queue;
		friend class io_service::timer_thread_state;

		io_service::schedule_operation m_scheduleOperation;
		std::chrono::high_resolution_clock::time_point m_resumeTime;

		cppcoro::cancellation_token m_cancellationToken;
		std::optional<cppcoro::cancellation_registration> m_cancellationRegistration;

		timed_schedule_operation* m_next;

		std::atomic<std::uint32_t> m_refCount;

	};

#elif CPPCORO_OS_LINUX || CPPCORO_OS_DARWIN
	class io_service::timed_schedule_operation
		: public detail::async_operation_cancellable<timed_schedule_operation>
	{
	public:

		timed_schedule_operation(
			io_service& service,
			std::chrono::high_resolution_clock::time_point resumeTime,
			cppcoro::cancellation_token&& ct) noexcept;

 	private:
 		friend class cppcoro::detail::async_operation_cancellable<timed_schedule_operation>;

 		bool try_start() noexcept;

		std::chrono::high_resolution_clock::time_point m_resumeTime;
	 	detail::safe_file_handle_t m_timerfd;
	};
#endif

	class io_work_scope
	{
	public:

		explicit io_work_scope(io_service& service) noexcept
			: m_service(&service)
		{
			service.notify_work_started();
		}

		io_work_scope(const io_work_scope& other) noexcept
			: m_service(other.m_service)
		{
			if (m_service != nullptr)
			{
				m_service->notify_work_started();
			}
		}

		io_work_scope(io_work_scope&& other) noexcept
			: m_service(other.m_service)
		{
			other.m_service = nullptr;
		}

		~io_work_scope()
		{
			if (m_service != nullptr)
			{
				m_service->notify_work_finished();
			}
		}

		void swap(io_work_scope& other) noexcept
		{
			std::swap(m_service, other.m_service);
		}

		io_work_scope& operator=(io_work_scope other) noexcept
		{
			swap(other);
			return *this;
		}

		io_service& service() noexcept
		{
			return *m_service;
		}

	private:

		io_service* m_service;

	};

	inline void swap(io_work_scope& a, io_work_scope& b)
	{
		a.swap(b);
	}
}

template<typename REP, typename RATIO>
cppcoro::io_service::timed_schedule_operation
cppcoro::io_service::schedule_after(
	const std::chrono::duration<REP, RATIO>& duration,
	cppcoro::cancellation_token cancellationToken) noexcept
{
	return timed_schedule_operation{
		*this,
		std::chrono::high_resolution_clock::now() + duration,
		std::move(cancellationToken)
	};
}

#endif
