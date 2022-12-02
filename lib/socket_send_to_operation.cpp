///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/net/socket_send_to_operation.hpp>
#include <cppcoro/net/socket.hpp>

#if CPPCORO_OS_WINNT
# include "socket_helpers.hpp"

# include <winsock2.h>
# include <ws2tcpip.h>
# include <mswsock.h>
# include <windows.h>

bool cppcoro::net::socket_send_to_operation_impl::try_start(
	cppcoro::detail::win32_overlapped_operation_base& operation) noexcept
{
	SOCKADDR_STORAGE destinationAddress;
	const int destinationLength = detail::ip_endpoint_to_sockaddr(
		m_destination, std::ref(destinationAddress));

	DWORD numberOfBytesSent = 0;
	int result = ::WSASendTo(
		m_socket.native_handle(),
		reinterpret_cast<WSABUF*>(&m_buffer),
		1, // buffer count
		&numberOfBytesSent,
		0, // flags
		reinterpret_cast<const SOCKADDR*>(&destinationAddress),
		destinationLength,
		operation.get_overlapped(),
		nullptr);
	if (result == SOCKET_ERROR)
	{
		int errorCode = ::WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			// Failed synchronously.
			operation.m_errorCode = static_cast<DWORD>(errorCode);
			operation.m_numberOfBytesTransferred = numberOfBytesSent;
			return false;
		}
	}

	// Operation will complete asynchronously.
	return true;
}

void cppcoro::net::socket_send_to_operation_impl::cancel(
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

#endif
