///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#include <system_error>

#include <cppcoro/net/socket_recv_from_operation.hpp>
#include <cppcoro/net/socket.hpp>
#include <cppcoro/io_service.hpp>

# include "socket_helpers.hpp"

#if CPPCORO_OS_WINNT
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <winsock2.h>
# include <ws2tcpip.h>
# include <windows.h>

bool cppcoro::net::socket_recv_from_operation_impl::try_start(
	cppcoro::detail::async_operation_base& operation) noexcept
{
	operation.m_handle = reinterpret_cast<HANDLE>(m_socket.native_handle());
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
	operation.m_completeFunc = [&]() {
		cppcoro::detail::win32::dword_t numberOfBytesTransferred = 0;
		cppcoro::detail::win32::dword_t flags = 0;
		cppcoro::detail::win32::bool_t ok = WSAGetOverlappedResult(
			m_socket.native_handle(),
			operation.get_overlapped(),
			&numberOfBytesTransferred,
			0,
			&flags
		);
		if (ok) {
			return std::make_tuple(static_cast<cppcoro::detail::win32::dword_t>(ERROR_SUCCESS), numberOfBytesTransferred);
		} else {
			return std::make_tuple(static_cast<cppcoro::detail::win32::dword_t>(WSAGetLastError()), numberOfBytesTransferred);
		}
	};

	// Operation will complete asynchronously.
	return true;
}

std::tuple<std::size_t, cppcoro::net::ip_endpoint>
cppcoro::net::socket_recv_from_operation_impl::get_result(
	cppcoro::detail::async_operation_base& operation)
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

#elif CPPCORO_OS_LINUX || CPPCORO_OS_DARWIN
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <netinet/udp.h>
bool cppcoro::net::socket_recv_from_operation_impl::try_start(
	cppcoro::detail::async_operation_base& operation) noexcept
{
	operation.m_fd = m_socket.native_handle();
	static_assert(
		sizeof(m_sourceSockaddrStorage) >= sizeof(sockaddr_in) &&
		sizeof(m_sourceSockaddrStorage) >= sizeof(sockaddr_in6));
	static_assert(
		sockaddrStorageAlignment >= alignof(sockaddr_in) &&
		sockaddrStorageAlignment >= alignof(sockaddr_in6));
	m_sourceSockaddrLength = sizeof(m_sourceSockaddrStorage);

	operation.m_completeFunc = [&]() {
#if !CPPCORO_OS_LINUX
		// NB: recvfrom(..., MSG_TRUNC) would be a more reliable way to do this on
		// Linux, but isn't supported by POSIX.
		int available;
		socklen_t optlen = sizeof(available);
		int err = getsockopt(m_socket.native_handle(), SOL_SOCKET, SO_NREAD, &available, &optlen);
		if (err != 0) {
			return -1;
		}
		if (available > m_byteCount) {
			errno = ENOMEM;
			return -1;
		}
#endif
		int res = recvfrom(
			m_socket.native_handle(),
			m_buffer,
			m_byteCount,
			MSG_TRUNC,
			reinterpret_cast<sockaddr*>(&m_sourceSockaddrStorage),
			reinterpret_cast<socklen_t*>(&m_sourceSockaddrLength));
#if !CPPCORO_OS_LINUX
		if (res > 0)
		{
			res = available;
		}
#endif
		return res;
	};
	operation.m_ioService->get_io_context().watch_handle(
		m_socket.native_handle(),
		reinterpret_cast<void*>(&operation),
		cppcoro::detail::watch_type::readable);
	return true;
}

std::tuple<std::size_t, cppcoro::net::ip_endpoint>
cppcoro::net::socket_recv_from_operation_impl::get_result(
	cppcoro::detail::async_operation_base& operation)
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
