///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Microsoft
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_MESSAGE_QUEUE_HPP_INCLUDED
#define CPPCORO_DETAIL_MESSAGE_QUEUE_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/detail/platform.hpp>
#include <cstdint>

namespace cppcoro
{
	namespace detail
	{
		enum class watch_type
		{
			readable,
			writable,
			readablewritable,
		};

		enum class message_type
		{
			CALLBACK_TYPE,
			RESUME_TYPE,
			WAKEUP_TYPE
		};

		struct message
		{
			message_type type;
			void* data;
		};

		class message_queue
		{
		public:
			explicit message_queue(std::uint32_t concurrencyHint);
			~message_queue();
			message_queue(message_queue&& other) = delete;
			message_queue& operator=(message_queue&& other) = delete;
			message_queue(const message_queue& other) = delete;
			message_queue& operator=(const message_queue& other) = delete;
			void add_handle(file_handle_t handle);
			void remove_handle(file_handle_t handle);
			void watch_handle(file_handle_t handle, void* cb, watch_type events);
			void unwatch_handle(file_handle_t handle);
			bool enqueue_message(message msg);
			bool dequeue_message(message& msg, bool wait);
#if CPPCORO_OS_DARWIN
			void watch_event(struct kevent* event, void* cb);
#endif

		private:
#if CPPCORO_OS_LINUX || CPPCORO_OS_DARWIN
			int m_pipefd[2];
#endif
			safe_file_handle_t m_pollfd;
		};

	}  // namespace detail
}  // namespace cppcoro

#endif
