///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Microsoft
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#include <cppcoro/detail/linux.hpp>
#include <cppcoro/io_service.hpp>
#include <system_error>
#include <unistd.h>
#include <cstring>
#include <cassert>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>


namespace cppcoro
{
	namespace detail
	{
		namespace linux
		{
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

			std::size_t io_state::get_result()
			{
				if (m_res < 0)
				{
					throw std::system_error{
						-m_res,
						std::system_category()
					};
				}

				return m_res;
			}

			void io_state::on_operation_completed()
			{
				m_ioService->get_io_context().unwatch_handle(m_fd);
				m_res = m_completeFunc();
				if (m_res < 0) {
					m_res = -errno;
				}
			}

			void io_state::cancel(void* operation) noexcept
			{
				m_ioService->get_io_context().unwatch_handle(m_fd);
				m_res = -ECANCELED;
				m_ioService->get_io_context().enqueue_message({
					message_type::RESUME_TYPE,
					operation
				});
			}
		}
	}
}
