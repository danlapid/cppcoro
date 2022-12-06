///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Microsoft
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_LINUX_HPP_INCLUDED
#define CPPCORO_DETAIL_LINUX_HPP_INCLUDED

#include <cppcoro/config.hpp>

#if !CPPCORO_OS_LINUX
# error <cppcoro/detail/linux.hpp> is only supported on the Linux platform.
#endif

#include <utility>
#include <functional>
#include <cstdint>

namespace cppcoro
{
	class io_service;
	namespace detail
	{
		namespace linux
		{
			using fd_t = int;

			class safe_fd
			{
			public:
				safe_fd()
					: m_fd(-1)
				{
				}

				explicit safe_fd(fd_t fd)
					: m_fd(fd)
				{
				}

				safe_fd(const safe_fd& other) = delete;

				safe_fd(safe_fd&& other) noexcept
					: m_fd(other.m_fd)
				{
					other.m_fd = -1;
				}

				~safe_fd() { close(); }

				safe_fd& operator=(safe_fd fd) noexcept
				{
					swap(fd);
					return *this;
				}

				constexpr fd_t fd() const { return m_fd; }
				constexpr fd_t handle() const { return m_fd; }

				/// Calls close() and sets the fd to -1.
				void close() noexcept;

				void swap(safe_fd& other) noexcept { std::swap(m_fd, other.m_fd); }

				bool operator==(const safe_fd& other) const { return m_fd == other.m_fd; }

				bool operator!=(const safe_fd& other) const { return m_fd != other.m_fd; }

				bool operator==(fd_t fd) const { return m_fd == fd; }

				bool operator!=(fd_t fd) const { return m_fd != fd; }

			private:
				fd_t m_fd;
			};

			struct io_state
			{
				io_state(io_service* ioService) noexcept
					: m_ioService(ioService)
					, m_fd(-1)
					, m_res(0)
				{}

				std::size_t get_result();
				void on_operation_completed_base();
				void cancel() noexcept;

				io_service* m_ioService;
				fd_t m_fd;
				std::int32_t m_res;
				std::function<int()> m_completeFunc;

			};

			safe_fd create_event_fd();
			safe_fd create_timer_fd();
			safe_fd create_epoll_fd();

		}  // namespace linux
	}      // namespace detail
}  // namespace cppcoro

#endif
