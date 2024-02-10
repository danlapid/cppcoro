///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Microsoft
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#include <cppcoro/detail/message_queue.hpp>
#include <cassert>
#include <cstring>
#include <fcntl.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>

namespace cppcoro
{
	namespace detail
	{
		message_queue::message_queue(std::uint32_t concurrencyHint)
			: m_pollfd(safe_file_handle_t{ darwin::create_kqueue_fd() })
		{
			if (pipe(m_pipefd) == -1)
			{
				throw std::system_error{ static_cast<int>(errno),
										 std::system_category(),
										 "Error creating io_service: failed creating pipe" };
			}
			if (fcntl(m_pipefd[0], F_SETFL, fcntl(m_pipefd[0], F_GETFL) | O_NONBLOCK) == -1)
			{
				throw std::system_error{
					static_cast<int>(errno),
					std::system_category(),
					"Error creating io_service: failed setting pipe to non blocking"
				};
			}
			if (fcntl(m_pipefd[1], F_SETFL, fcntl(m_pipefd[1], F_GETFL) | O_NONBLOCK) == -1)
			{
				throw std::system_error{
					static_cast<int>(errno),
					std::system_category(),
					"Error creating io_service: failed setting pipe to non blocking"
				};
			}
			watch_handle(m_pipefd[0], reinterpret_cast<void*>(m_pipefd[0]), watch_type::readable);
		}

		message_queue::~message_queue()
		{
			unwatch_handle(m_pipefd[0]);
			assert(close(m_pipefd[0]) == 0);
			assert(close(m_pipefd[1]) == 0);
		}

		void message_queue::add_handle(file_handle_t handle)
		{
		}
		void message_queue::remove_handle(file_handle_t handle)
		{
		}

		void message_queue::watch_handle(file_handle_t handle, void* cb, watch_type events)
		{
			struct kevent ev;
			switch (events)
			{
				case watch_type::readable:
					EV_SET(&ev, handle, EVFILT_READ, EV_ADD, 0, 0, cb);
					watch_event(&ev, cb);
					break;
				case watch_type::writable:
					EV_SET(&ev, handle, EVFILT_WRITE, EV_ADD, 0, 0, cb);
					watch_event(&ev, cb);
					break;
				case watch_type::readablewritable:
					EV_SET(&ev, handle, EVFILT_READ, EV_ADD, 0, 0, cb);
					watch_event(&ev, cb);
					EV_SET(&ev, handle, EVFILT_WRITE, EV_ADD, 0, 0, cb);
					watch_event(&ev, cb);
					break;
			}
		}
		void message_queue::watch_event(struct kevent* event, void* cb)
		{
			if (kevent(m_pollfd.fd(), event, 1, NULL, 0, NULL) == -1)
			{
				if (errno == EPERM)
				{
					// epoll returns EPERM on regular files because they are
					// always ready for read/write, we can just queue the callback to run
					enqueue_message({ message_type::CALLBACK_TYPE, cb });
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
			if (handle == -1)
			{
				return;
			}
			struct kevent ev;
			EV_SET(&ev, handle, EVFILT_READ, EV_DELETE, 0, 0, NULL);
			if (kevent(m_pollfd.fd(), &ev, 1, NULL, 0, NULL) == -1)
			{
				if (errno != EPERM && errno != ENOENT)
				{
					throw std::system_error{ static_cast<int>(errno),
											 std::system_category(),
											 "message_queue: unwatch_handle failed" };
				}
			}
			EV_SET(&ev, handle, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
			if (kevent(m_pollfd.fd(), &ev, 1, NULL, 0, NULL) == -1)
			{
				if (errno != EPERM && errno != ENOENT)
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
			struct kevent ev;
			struct timespec immediate = { 0, 0 };
			int nfds = kevent(m_pollfd.fd(), NULL, 0, &ev, 1, wait ? NULL : &immediate);
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

			if (static_cast<file_handle_t>(ev.ident) == m_pipefd[0])
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
				msg.data = ev.udata;
				msg.type = message_type::CALLBACK_TYPE;
				return true;
			}
		}
	}  // namespace detail
}  // namespace cppcoro
