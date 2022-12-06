///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_NET_SOCKET_DISCONNECT_OPERATION_HPP_INCLUDED
#define CPPCORO_NET_SOCKET_DISCONNECT_OPERATION_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/cancellation_token.hpp>

#include <cppcoro/detail/platform.hpp>
#include <cppcoro/detail/async_operation.hpp>

namespace cppcoro
{
	namespace net
	{
		class socket;

		class socket_disconnect_operation_impl
		{
		public:

			socket_disconnect_operation_impl(socket& socket) noexcept
				: m_socket(socket)
			{}

			bool try_start(cppcoro::detail::async_operation_base& operation) noexcept;
			void cancel(cppcoro::detail::async_operation_base& operation) noexcept;
			void get_result(cppcoro::detail::async_operation_base& operation);

		private:

			socket& m_socket;

		};

		class socket_disconnect_operation
			: public cppcoro::detail::async_operation<socket_disconnect_operation>
		{
		public:

			socket_disconnect_operation(
				socket& socket,
				cppcoro::io_service* ioService) noexcept
				: cppcoro::detail::async_operation<socket_disconnect_operation>(ioService)
				, m_impl(socket)
			{}

		private:

			friend class cppcoro::detail::async_operation<socket_disconnect_operation>;
			friend class cppcoro::detail::has_get_result<socket_disconnect_operation>;

			bool try_start() noexcept { return m_impl.try_start(*this); }
			decltype(auto) get_result() { return m_impl.get_result(*this); }

			socket_disconnect_operation_impl m_impl;

		};

		class socket_disconnect_operation_cancellable
			: public cppcoro::detail::async_operation_cancellable<socket_disconnect_operation_cancellable>
		{
		public:

			socket_disconnect_operation_cancellable(
				socket& socket,
				cppcoro::io_service* ioService,
				cancellation_token&& ct) noexcept
				: cppcoro::detail::async_operation_cancellable<socket_disconnect_operation_cancellable>(ioService, std::move(ct))
				, m_impl(socket)
			{}

		private:

			friend class cppcoro::detail::async_operation_cancellable<socket_disconnect_operation_cancellable>;
			friend class cppcoro::detail::has_get_result<socket_disconnect_operation_cancellable>;

			bool try_start() noexcept { return m_impl.try_start(*this); }
			decltype(auto) get_result() { return m_impl.get_result(*this); }

			socket_disconnect_operation_impl m_impl;

		};
	}
}


#endif
