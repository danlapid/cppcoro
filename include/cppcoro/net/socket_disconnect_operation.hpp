///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_NET_SOCKET_DISCONNECT_OPERATION_HPP_INCLUDED
#define CPPCORO_NET_SOCKET_DISCONNECT_OPERATION_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/cancellation_token.hpp>

#if CPPCORO_OS_WINNT
# include <cppcoro/detail/win32.hpp>
# include <cppcoro/detail/win32_overlapped_operation.hpp>
#elif CPPCORO_OS_LINUX
# include <cppcoro/detail/linux.hpp>
# include <cppcoro/detail/linux_async_operation.hpp>
#endif

namespace cppcoro
{
	namespace net
	{
#if CPPCORO_OS_WINNT
		class socket;

		class socket_disconnect_operation_impl
		{
		public:

			socket_disconnect_operation_impl(socket& socket) noexcept
				: m_socket(socket)
			{}

			bool try_start(cppcoro::detail::win32_overlapped_operation_base& operation) noexcept;
			void cancel(cppcoro::detail::win32_overlapped_operation_base& operation) noexcept;
			void get_result(cppcoro::detail::win32_overlapped_operation_base& operation);

		private:

			socket& m_socket;

		};

		class socket_disconnect_operation
			: public cppcoro::detail::win32_overlapped_operation<socket_disconnect_operation>
		{
		public:

			socket_disconnect_operation(socket& socket) noexcept
				: m_impl(socket)
			{}

		private:

			friend class cppcoro::detail::win32_overlapped_operation<socket_disconnect_operation>;

			bool try_start() noexcept { return m_impl.try_start(*this); }
			void get_result() { m_impl.get_result(*this); }

			socket_disconnect_operation_impl m_impl;

		};

		class socket_disconnect_operation_cancellable
			: public cppcoro::detail::win32_overlapped_operation_cancellable<socket_disconnect_operation_cancellable>
		{
		public:

			socket_disconnect_operation_cancellable(socket& socket, cancellation_token&& ct) noexcept
				: cppcoro::detail::win32_overlapped_operation_cancellable<socket_disconnect_operation_cancellable>(std::move(ct))
				, m_impl(socket)
			{}

		private:

			friend class cppcoro::detail::win32_overlapped_operation_cancellable<socket_disconnect_operation_cancellable>;

			bool try_start() noexcept { return m_impl.try_start(*this); }
			void cancel() noexcept { m_impl.cancel(*this); }
			void get_result() { m_impl.get_result(*this); }

			socket_disconnect_operation_impl m_impl;

		};
#elif CPPCORO_OS_LINUX
		class socket;

		class socket_disconnect_operation_impl
		{
		public:

			socket_disconnect_operation_impl(socket& socket) noexcept
				: m_socket(socket)
			{}

			bool try_start(cppcoro::detail::linux_async_operation_base& operation) noexcept;
			void cancel(cppcoro::detail::linux_async_operation_base& operation) noexcept;
			void get_result(cppcoro::detail::linux_async_operation_base& operation);

		private:

			socket& m_socket;

		};

		class socket_disconnect_operation
			: public cppcoro::detail::linux_async_operation<socket_disconnect_operation>
		{
		public:

			socket_disconnect_operation(
				socket& socket,
				cppcoro::detail::linux::message_queue* mq) noexcept
				: cppcoro::detail::linux_async_operation<socket_disconnect_operation>(mq)
				, m_impl(socket)
			{}

		private:

			friend class cppcoro::detail::linux_async_operation<socket_disconnect_operation>;

			bool try_start() noexcept { return m_impl.try_start(*this); }
			void get_result() { m_impl.get_result(*this); }

			socket_disconnect_operation_impl m_impl;

		};

		class socket_disconnect_operation_cancellable
			: public cppcoro::detail::linux_async_operation_cancellable<socket_disconnect_operation_cancellable>
		{
		public:

			socket_disconnect_operation_cancellable(
				socket& socket,
				cppcoro::detail::linux::message_queue* mq,
				cancellation_token&& ct) noexcept
				: cppcoro::detail::linux_async_operation_cancellable<socket_disconnect_operation_cancellable>(mq, std::move(ct))
				, m_impl(socket)
			{}

		private:

			friend class cppcoro::detail::linux_async_operation_cancellable<socket_disconnect_operation_cancellable>;

			bool try_start() noexcept { return m_impl.try_start(*this); }
			void cancel() noexcept { m_impl.cancel(*this); }
			void get_result() { m_impl.get_result(*this); }

			socket_disconnect_operation_impl m_impl;

		};
#endif
	}
}


#endif
