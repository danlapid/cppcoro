///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_NET_SOCKET_ACCEPT_OPERATION_HPP_INCLUDED
#define CPPCORO_NET_SOCKET_ACCEPT_OPERATION_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/cancellation_token.hpp>
#include <cppcoro/cancellation_registration.hpp>

#include <atomic>
#include <optional>

#include <cppcoro/detail/platform.hpp>
#include <cppcoro/detail/async_operation.hpp>

namespace cppcoro
{
	namespace net
	{
		class socket;

		class socket_accept_operation_impl
		{
		public:

			socket_accept_operation_impl(
				socket& listeningSocket,
				socket& acceptingSocket) noexcept
				: m_listeningSocket(listeningSocket)
				, m_acceptingSocket(acceptingSocket)
			{}

			bool try_start(cppcoro::detail::async_operation_base& operation) noexcept;
			void cancel(cppcoro::detail::async_operation_base& operation) noexcept;
			void get_result(cppcoro::detail::async_operation_base& operation);

		private:
#if CPPCORO_COMPILER_MSVC
# pragma warning(push)
# pragma warning(disable : 4324) // Structure padded due to alignment
#endif
			socket& m_listeningSocket;
			socket& m_acceptingSocket;
			alignas(8) std::uint8_t m_addressBuffer[88];
#if CPPCORO_COMPILER_MSVC
# pragma warning(pop)
#endif
		};

		class socket_accept_operation
			: public cppcoro::detail::async_operation<socket_accept_operation>
		{
		public:

			socket_accept_operation(
				socket& listeningSocket,
				socket& acceptingSocket,
				cppcoro::io_service* ioService) noexcept
				: cppcoro::detail::async_operation<socket_accept_operation>(ioService)
				, m_impl(listeningSocket, acceptingSocket)
			{}

		private:

			friend class cppcoro::detail::async_operation<socket_accept_operation>;

			bool try_start() noexcept { return m_impl.try_start(*this); }
			decltype(auto) get_result() { return m_impl.get_result(*this); }

			socket_accept_operation_impl m_impl;

		};

		class socket_accept_operation_cancellable
			: public cppcoro::detail::async_operation_cancellable<socket_accept_operation_cancellable>
		{
		public:

			socket_accept_operation_cancellable(
				socket& listeningSocket,
				socket& acceptingSocket,
				cppcoro::io_service* ioService,
				cancellation_token&& ct) noexcept
				: cppcoro::detail::async_operation_cancellable<socket_accept_operation_cancellable>(ioService, std::move(ct))
				, m_impl(listeningSocket, acceptingSocket)
			{}

		private:

			friend class cppcoro::detail::async_operation_cancellable<socket_accept_operation_cancellable>;

			bool try_start() noexcept { return m_impl.try_start(*this); }
			decltype(auto) get_result() { return m_impl.get_result(*this); }

			socket_accept_operation_impl m_impl;

		};
	}
}

#endif
