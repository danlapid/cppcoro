///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/net/socket_recv_from_operation.hpp>
#include <cppcoro/net/socket.hpp>

# include "socket_helpers.hpp"

#if CPPCORO_OS_WINNT
# include <winsock2.h>
# include <ws2tcpip.h>
# include <mswsock.h>
# include <windows.h>

bool cppcoro::net::socket_recv_from_operation_impl::try_start(
	cppcoro::detail::win32_overlapped_operation_base& operation) noexcept
{
	static_assert(
		sizeof(m_sourceSockaddrStorage) >= sizeof(SOCKADDR_IN) &&
		sizeof(m_sourceSockaddrStorage) >= sizeof(SOCKADDR_IN6));
	static_assert(
		sockaddrStorageAlignment >= alignof(SOCKADDR_IN) &&
		sockaddrStorageAlignment >= alignof(SOCKADDR_IN6));

	m_sourceSockaddrLength = sizeof(m_sourceSockaddrStorage);

	DWORD numberOfBytesReceived = 0;
	DWORD flags = 0;
	int result = ::WSARecvFrom(
		m_socket.native_handle(),
		reinterpret_cast<WSABUF*>(&m_buffer),
		1, // buffer count
		&numberOfBytesReceived,
		&flags,
		reinterpret_cast<sockaddr*>(&m_sourceSockaddrStorage),
		&m_sourceSockaddrLength,
		operation.get_overlapped(),
		nullptr);
	if (result == SOCKET_ERROR)
	{
		int errorCode = ::WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			// Failed synchronously.
			operation.m_errorCode = static_cast<DWORD>(errorCode);
			operation.m_numberOfBytesTransferred = numberOfBytesReceived;
			return false;
		}
	}
	auto socketHandle = m_socket.native_handle();
	auto* overlapped = operation.get_overlapped();
	operation.m_completeFunc = [socketHandle, overlapped]() -> int64_t {
		cppcoro::detail::win32::dword_t numberOfBytesTransferred = 0;
		cppcoro::detail::win32::bool_t ok;
		cppcoro::detail::win32::dword_t flags;
		ok = WSAGetOverlappedResult(
			socketHandle,
			overlapped,
			&numberOfBytesTransferred,
			0,
			&flags
		);
		if (ok) {
			return numberOfBytesTransferred;
		} else {
			return -WSAGetLastError();
		}
	};

	// Operation will complete asynchronously.
	return true;
}

void cppcoro::net::socket_recv_from_operation_impl::cancel(
	cppcoro::detail::win32_overlapped_operation_base& operation) noexcept
{
#if CPPCORO_OS_WINNT >= 0x600
	(void)::CancelIoEx(
		reinterpret_cast<HANDLE>(m_socket.native_handle()),
		operation.get_overlapped());
#else
	(void)::CancelIo(
		reinterpret_cast<HANDLE>(m_socket.native_handle()));
#endif
}

std::tuple<std::size_t, cppcoro::net::ip_endpoint>
cppcoro::net::socket_recv_from_operation_impl::get_result(
	cppcoro::detail::win32_overlapped_operation_base& operation)
{
	if (operation.m_errorCode != ERROR_SUCCESS)
	{
		throw std::system_error(
			static_cast<int>(operation.m_errorCode),
			std::system_category(),
			"Error receiving message on socket: WSARecvFrom");
	}

	return std::make_tuple(
		static_cast<std::size_t>(operation.m_numberOfBytesTransferred),
		detail::sockaddr_to_ip_endpoint(
			*reinterpret_cast<SOCKADDR*>(&m_sourceSockaddrStorage)));
}

#elif CPPCORO_OS_LINUX
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <netinet/udp.h>
bool cppcoro::net::socket_recv_from_operation_impl::try_start(
	cppcoro::detail::linux_async_operation_base& operation) noexcept
{
	static_assert(
		sizeof(m_sourceSockaddrStorage) >= sizeof(sockaddr_in) &&
		sizeof(m_sourceSockaddrStorage) >= sizeof(sockaddr_in6));
	static_assert(
		sockaddrStorageAlignment >= alignof(sockaddr_in) &&
		sockaddrStorageAlignment >= alignof(sockaddr_in6));
	m_sourceSockaddrLength = sizeof(m_sourceSockaddrStorage);

	operation.m_completeFunc = [&]() {
		int res = recvfrom(
			m_socket.native_handle(), m_buffer, m_byteCount, MSG_TRUNC,
			reinterpret_cast<sockaddr*>(&m_sourceSockaddrStorage),
			reinterpret_cast<socklen_t*>(&m_sourceSockaddrLength)
		);
		operation.m_ioService->get_io_context().unwatch_handle(m_socket.native_handle());
		return res;
	};
	operation.m_ioService->get_io_context().watch_handle(m_socket.native_handle(), reinterpret_cast<void*>(&operation), cppcoro::detail::watch_type::readable);
	return true;
}

void cppcoro::net::socket_recv_from_operation_impl::cancel(
	cppcoro::detail::linux_async_operation_base& operation) noexcept
{
	operation.m_ioService->get_io_context().unwatch_handle(m_socket.native_handle());
}

std::tuple<std::size_t, cppcoro::net::ip_endpoint>
cppcoro::net::socket_recv_from_operation_impl::get_result(
	cppcoro::detail::linux_async_operation_base& operation)
{
	if (operation.m_res < 0)
	{
		throw std::system_error(
			static_cast<int>(-operation.m_res),
			std::system_category(),
			"Error receiving message on socket: recvfrom");
	}
	if (operation.m_res > m_byteCount) {
		throw std::system_error(
			ENOMEM,
			std::system_category(),
			"Error receiving message on socket: recvfrom - receiving buffer was too small");

	}

	return std::make_tuple(
		static_cast<std::size_t>(operation.m_res),
		detail::sockaddr_to_ip_endpoint(
			*reinterpret_cast<sockaddr*>(&m_sourceSockaddrStorage)));
}
#endif
