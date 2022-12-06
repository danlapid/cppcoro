///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/net/socket_recv_operation.hpp>
#include <cppcoro/net/socket.hpp>
#include <cppcoro/io_service.hpp>

#if CPPCORO_OS_WINNT
# include <winsock2.h>
# include <ws2tcpip.h>
# include <mswsock.h>
# include <windows.h>

bool cppcoro::net::socket_recv_operation_impl::try_start(
	cppcoro::detail::async_operation_base& operation) noexcept
{
	operation.m_ioState.m_handle = reinterpret_cast<HANDLE>(m_socket.native_handle());
	DWORD numberOfBytesReceived = 0;
	DWORD flags = 0;
	int result = ::WSARecv(
		m_socket.native_handle(),
		reinterpret_cast<WSABUF*>(&m_buffer),
		1, // buffer count
		&numberOfBytesReceived,
		&flags,
		operation.m_ioState.get_overlapped(),
		nullptr);
	if (result == SOCKET_ERROR)
	{
		int errorCode = ::WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			// Failed synchronously.
			operation.m_ioState.m_errorCode = static_cast<DWORD>(errorCode);
			operation.m_ioState.m_numberOfBytesTransferred = numberOfBytesReceived;
			return false;
		}
	}
	operation.m_ioState.m_completeFunc = [&]() -> int64_t {
		cppcoro::detail::win32::dword_t numberOfBytesTransferred = 0;
		cppcoro::detail::win32::bool_t ok;
		cppcoro::detail::win32::dword_t flags;
		ok = WSAGetOverlappedResult(
			m_socket.native_handle(),
			operation.m_ioState.get_overlapped(),
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

#elif CPPCORO_OS_LINUX
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <netinet/udp.h>

bool cppcoro::net::socket_recv_operation_impl::try_start(
	cppcoro::detail::async_operation_base& operation) noexcept
{
	operation.m_ioState.m_fd = m_socket.native_handle();
	operation.m_ioState.m_completeFunc = [&]() {
		return recv(m_socket.native_handle(), m_buffer, m_byteCount, 0);
	};
	operation.m_ioState.m_ioService->get_io_context().watch_handle(m_socket.native_handle(), reinterpret_cast<void*>(&operation), detail::watch_type::readable);
	return true;
}
#endif
