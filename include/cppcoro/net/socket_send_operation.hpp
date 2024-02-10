///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_NET_SOCKET_SEND_OPERATION_HPP_INCLUDED
#define CPPCORO_NET_SOCKET_SEND_OPERATION_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/cancellation_token.hpp>

#include <cstdint>

#include <cppcoro/detail/platform.hpp>
#include <cppcoro/detail/async_operation.hpp>

namespace cppcoro::net
{
	class socket;

	class socket_send_operation_impl
	{
	public:

		socket_send_operation_impl(
			socket& s,
			const void* buffer,
			std::size_t byteCount) noexcept
			: m_socket(s)
#if CPPCORO_OS_WINNT
			, m_buffer(const_cast<void*>(buffer), byteCount)
#elif CPPCORO_OS_LINUX
			, m_buffer(buffer)
			, m_byteCount(byteCount)
#endif
		{}

		bool try_start(cppcoro::detail::async_operation_base& operation) noexcept;
		void cancel(cppcoro::detail::async_operation_base& operation) noexcept;

	private:

		socket& m_socket;
#if CPPCORO_OS_WINNT
		cppcoro::detail::win32::wsabuf m_buffer;
#elif CPPCORO_OS_LINUX
		const void* m_buffer;
		std::size_t m_byteCount;
#endif

	};

	class socket_send_operation
		: public cppcoro::detail::async_operation<socket_send_operation>
	{
	public:

		socket_send_operation(
			socket& s,
			const void* buffer,
			std::size_t byteCount,
			cppcoro::io_service* ioService) noexcept
			: cppcoro::detail::async_operation<socket_send_operation>(ioService)
			, m_impl(s, buffer, byteCount)
		{}

	private:

		friend class cppcoro::detail::async_operation<socket_send_operation>;

		bool try_start() noexcept { return m_impl.try_start(*this); }

		socket_send_operation_impl m_impl;

	};

	class socket_send_operation_cancellable
		: public cppcoro::detail::async_operation_cancellable<socket_send_operation_cancellable>
	{
	public:

		socket_send_operation_cancellable(
			socket& s,
			const void* buffer,
			std::size_t byteCount,
			cppcoro::io_service* ioService,
			cancellation_token&& ct) noexcept
			: cppcoro::detail::async_operation_cancellable<socket_send_operation_cancellable>(ioService, std::move(ct))
			, m_impl(s, buffer, byteCount)
		{}

	private:

		friend class cppcoro::detail::async_operation_cancellable<socket_send_operation_cancellable>;

		bool try_start() noexcept { return m_impl.try_start(*this); }

		socket_send_operation_impl m_impl;

	};
}

#endif
