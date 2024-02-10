///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_NET_SOCKET_CONNECT_OPERATION_HPP_INCLUDED
#define CPPCORO_NET_SOCKET_CONNECT_OPERATION_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/cancellation_token.hpp>
#include <cppcoro/net/ip_endpoint.hpp>

#include <cppcoro/detail/platform.hpp>
#include <cppcoro/detail/async_operation.hpp>

namespace cppcoro
{
	namespace net
	{
		class socket;

		class socket_connect_operation_impl
		{
		public:

			socket_connect_operation_impl(
				socket& socket,
				const ip_endpoint& remoteEndPoint) noexcept
				: m_socket(socket)
				, m_remoteEndPoint(remoteEndPoint)
			{}

			bool try_start(cppcoro::detail::async_operation_base& operation) noexcept;
			void cancel(cppcoro::detail::async_operation_base& operation) noexcept;
			void get_result(cppcoro::detail::async_operation_base& operation);

		private:

			socket& m_socket;
			ip_endpoint m_remoteEndPoint;

		};

		class socket_connect_operation
			: public cppcoro::detail::async_operation<socket_connect_operation>
		{
		public:

			socket_connect_operation(
				socket& socket,
				const ip_endpoint& remoteEndPoint,
				cppcoro::io_service* ioService) noexcept
				: cppcoro::detail::async_operation<socket_connect_operation>(ioService)
				, m_impl(socket, remoteEndPoint)
			{}

		private:

			friend class cppcoro::detail::async_operation<socket_connect_operation>;

			bool try_start() noexcept { return m_impl.try_start(*this); }
			decltype(auto) get_result() { return m_impl.get_result(*this); }

			socket_connect_operation_impl m_impl;

		};

		class socket_connect_operation_cancellable
			: public cppcoro::detail::async_operation_cancellable<socket_connect_operation_cancellable>
		{
		public:

			socket_connect_operation_cancellable(
				socket& socket,
				const ip_endpoint& remoteEndPoint,
				cppcoro::io_service* ioService,
				cancellation_token&& ct) noexcept
				: cppcoro::detail::async_operation_cancellable<socket_connect_operation_cancellable>(ioService, std::move(ct))
				, m_impl(socket, remoteEndPoint)
			{}

		private:

			friend class cppcoro::detail::async_operation_cancellable<socket_connect_operation_cancellable>;

			bool try_start() noexcept { return m_impl.try_start(*this); }
			decltype(auto) get_result() { return m_impl.get_result(*this); }

			socket_connect_operation_impl m_impl;

		};
	}
}


#endif
