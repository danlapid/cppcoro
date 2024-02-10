///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Microsoft
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_DARWIN_HPP_INCLUDED
#define CPPCORO_DETAIL_DARWIN_HPP_INCLUDED

#include <cppcoro/config.hpp>

#if !CPPCORO_OS_DARWIN
#error <cppcoro/detail/darwin.hpp> is only supported on the Linux platform.
#endif

#include <cstdint>
#include <functional>
#include <sys/event.h>
#include <utility>

namespace cppcoro
{
	class io_service;
	namespace detail
	{
		namespace darwin
		{
			using fd_t = int;

			class safe_fd
			{
			public:
				safe_fd();

				explicit safe_fd(fd_t fd);
				~safe_fd() noexcept;
				safe_fd(const safe_fd& other) noexcept;
				safe_fd& operator=(const safe_fd& other) noexcept;
				safe_fd(safe_fd&& other) noexcept;
				safe_fd& operator=(safe_fd&& other) noexcept;
				constexpr fd_t fd() const { return m_fd; }
				constexpr fd_t handle() const { return m_fd; }
				/// Calls close() and sets the fd to -1.
				void close() noexcept;
				bool operator==(const safe_fd& other) const { return m_fd == other.m_fd; }
				bool operator!=(const safe_fd& other) const { return m_fd != other.m_fd; }
				bool operator==(fd_t fd) const { return m_fd == fd; }
				bool operator!=(fd_t fd) const { return m_fd != fd; }

			private:
				fd_t m_fd;
			};

			struct io_state
			{
				explicit io_state(io_service* ioService) noexcept
					: m_ioService(ioService)
					, m_fd(-1)
					, m_res(0)
					, m_completeFunc([] { return 0; })
				{
				}

				std::size_t get_result();
				void on_operation_completed_base();
				void cancel() noexcept;

				io_service* m_ioService;
				fd_t m_fd;
				std::int32_t m_res;
				std::function<int()> m_completeFunc;
			};

			safe_fd create_timer_fd();
			safe_fd create_kqueue_fd();

		}  // namespace darwin
	}      // namespace detail
}  // namespace cppcoro

#endif
