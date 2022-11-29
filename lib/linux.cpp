///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Microsoft
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#include <cppcoro/detail/linux.hpp>
#include <system_error>
#include <unistd.h>
#include <cstring>
#include <cassert>

namespace cppcoro
{
	namespace detail
	{
		namespace linux
		{
			message_queue::message_queue()
			{
				if (pipe2(m_pipefd, O_NONBLOCK) == -1) {
					throw std::system_error
					{
						static_cast<int>(errno),
						std::system_category(),
						"Error creating io_service: failed creating pipe"
					};
				}
				m_epollfd = safe_fd{create_epoll_fd()};
				m_ev.data.fd = m_pipefd[0];
				m_ev.events = EPOLLIN;

				if(epoll_ctl(m_epollfd.fd(), EPOLL_CTL_ADD, m_pipefd[0], &m_ev) == -1)
				{
					throw std::system_error
					{
						static_cast<int>(errno),
						std::system_category(),
						"Error creating io_service: epoll ctl pipe"
					};
				}
			}

			message_queue::~message_queue()
			{
				assert(close(m_pipefd[0]) == 0);
				assert(close(m_pipefd[1]) == 0);
			}

			bool message_queue::enqueue_message(void* msg, message_type type)
			{
				message qmsg;
				qmsg.m_type = type;
				qmsg.m_ptr = msg;
				int status = write(m_pipefd[1], (const char*)&qmsg, sizeof(message));
				return status==-1?false:true;
			}

			bool message_queue::dequeue_message(void*& msg, message_type& type, bool wait)
			{
				struct epoll_event ev = {0};
				int nfds = epoll_wait(m_epollfd.fd(), &ev, 1, wait?-1:0);

				if(nfds == -1)
				{
					if (errno == EINTR || errno == EAGAIN) {
						return false;
					}
					throw std::system_error
					{
						static_cast<int>(errno),
						std::system_category(),
						"Error in epoll_wait run loop"
					};
				}

				if(nfds == 0 && !wait)
				{
					return false;
				}

				if(nfds == 0 && wait)
				{
					throw std::system_error
					{
						static_cast<int>(errno),
						std::system_category(),
						"Error in epoll_wait run loop"
					};
				}

				message qmsg;
				ssize_t status = read(m_pipefd[0], (char*)&qmsg, sizeof(message));

				if(status == -1)
				{
					if (errno == EINTR || errno == EAGAIN) {
						return false;
					}
					throw std::system_error
					{
						static_cast<int>(errno),
						std::system_category(),
						"Error retrieving message from message queue: mq_receive"
					};
				}

				msg = qmsg.m_ptr;
				type = qmsg.m_type;
				return true;
			}

			safe_fd create_event_fd()
			{
				int fd = eventfd(0, EFD_SEMAPHORE | EFD_NONBLOCK | EFD_CLOEXEC);

				if(fd == -1)
				{
					throw std::system_error
					{
						static_cast<int>(errno),
						std::system_category(),
						"Error creating io_service: event fd create"
					};
				}

				return safe_fd{fd};
			}

			safe_fd create_timer_fd()
			{
				int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

				if(fd == -1)
				{
					throw std::system_error
					{
						static_cast<int>(errno),
						std::system_category(),
						"Error creating io_service: timer fd create"
					};
				}

				return safe_fd{fd};
			}

			safe_fd create_epoll_fd()
			{
				int fd = epoll_create1(EPOLL_CLOEXEC);

				if(fd == -1)
				{
					throw std::system_error
					{
						static_cast<int>(errno),
						std::system_category(),
						"Error creating timer thread: epoll create"
					};
				}

				return safe_fd{fd};
			}

			void safe_fd::close() noexcept
			{
				if(m_fd != -1)
				{
					::close(m_fd);
					m_fd = -1;
				}
			}
		}
	}
}
