///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Microsoft
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#include <cppcoro/detail/message_queue.hpp>
#include <cassert>
#include <cstring>
#include <system_error>
#include <unistd.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>


namespace cppcoro
{
	namespace detail
	{
		message_queue::message_queue(std::uint32_t concurrencyHint)
			: m_pollfd(safe_file_handle_t{ linux::create_epoll_fd() })
		{
			if (pipe2(m_pipefd, O_NONBLOCK) == -1)
			{
				throw std::system_error{ static_cast<int>(errno),
										 std::system_category(),
										 "Error creating io_service: failed creating pipe" };
			}
			watch_handle(m_pipefd[0], reinterpret_cast<void*>(m_pipefd[0]), watch_type::readable);
		}

		message_queue::~message_queue()
		{
			try {
			unwatch_handle(m_pipefd[0]);
			} catch (...) {/* intentionally left empty */}
			assert(close(m_pipefd[0]) == 0);
			assert(close(m_pipefd[1]) == 0);
		}

        void message_queue::add_handle(file_handle_t handle) {}
        void message_queue::remove_handle(file_handle_t handle) {}

		void message_queue::watch_handle(file_handle_t handle, void* cb, watch_type events)
		{
			struct epoll_event ev = { 0 };
			switch (events)
			{
			case watch_type::readable:
				ev.events = EPOLLIN;
				break;
			case watch_type::writable:
				ev.events = EPOLLOUT;
				break;
			case watch_type::readablewritable:
				ev.events = EPOLLIN | EPOLLOUT;
				break;
			}
			ev.data.ptr = cb;
			if (epoll_ctl(m_pollfd.fd(), EPOLL_CTL_ADD, handle, &ev) == -1)
			{
				if (errno == EPERM)
				{
					// epoll returns EPERM on regular files because they are
					// always ready for read/write, we can just queue the callback to run
					enqueue_message({message_type::CALLBACK_TYPE, cb});
				}
				else
				{
					throw std::system_error{ static_cast<int>(errno),
											 std::system_category(),
											 "message_queue: watch_handle failed" };
				}
			}
		}

		void message_queue::unwatch_handle(file_handle_t handle)
		{
			if (epoll_ctl(m_pollfd.fd(), EPOLL_CTL_DEL, handle, NULL) == -1)
			{
				if (errno != EPERM)
				{
					throw std::system_error{ static_cast<int>(errno),
											 std::system_category(),
											 "message_queue: unwatch_handle failed" };
				}
			}
		}

		bool message_queue::enqueue_message(message msg)
		{
			int status = write(m_pipefd[1], (const char*)&msg, sizeof(msg));
			return status == -1 ? false : true;
		}

		bool message_queue::dequeue_message(message& msg, bool wait)
		{
			struct epoll_event ev = { 0 };
			int nfds = epoll_wait(m_pollfd.fd(), &ev, 1, wait ? -1 : 0);

			if (nfds == -1)
			{
				if (errno == EINTR || errno == EAGAIN)
				{
					return false;
				}
				throw std::system_error{ static_cast<int>(errno),
										 std::system_category(),
										 "Error in epoll_wait run loop" };
			}

			if (nfds == 0 && !wait)
			{
				return false;
			}

			if (nfds == 0 && wait)
			{
				throw std::system_error{ static_cast<int>(errno),
										 std::system_category(),
										 "Error in epoll_wait run loop" };
			}

			if (ev.data.fd == m_pipefd[0])
			{
				ssize_t status = read(m_pipefd[0], (char*)&msg, sizeof(msg));

				if (status == -1)
				{
					if (errno == EINTR || errno == EAGAIN)
					{
						return false;
					}
					throw std::system_error{ static_cast<int>(errno),
											 std::system_category(),
											 "Error retrieving message from message queue" };
				}

				return true;
			}
			else
			{
				msg.data = ev.data.ptr;
				msg.type = message_type::CALLBACK_TYPE;
				return true;
			}
		}
	}  // namespace detail
}  // namespace cppcoro
