///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/net/socket_recv_operation.hpp>
#include <cppcoro/net/socket.hpp>

#if CPPCORO_OS_WINNT
# include <winsock2.h>
# include <ws2tcpip.h>
# include <mswsock.h>
# include <windows.h>

bool cppcoro::net::socket_recv_operation_impl::try_start(
	cppcoro::detail::win32_overlapped_operation_base& operation) noexcept
{
	DWORD numberOfBytesReceived = 0;
	DWORD flags = 0;
	int result = ::WSARecv(
		m_socket.native_handle(),
		reinterpret_cast<WSABUF*>(&m_buffer),
		1, // buffer count
		&numberOfBytesReceived,
		&flags,
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


void cppcoro::net::socket_recv_operation_impl::cancel(
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

#elif CPPCORO_OS_LINUX
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <netinet/udp.h>

bool cppcoro::net::socket_recv_operation_impl::try_start(
	cppcoro::detail::linux_async_operation_base& operation) noexcept
{
	operation.m_completeFunc = [&]() {
		int res = recv(m_socket.native_handle(), m_buffer, m_byteCount, 0);
		operation.m_ioService->get_io_context().unwatch_handle(m_socket.native_handle());
		return res;
	};
	operation.m_ioService->get_io_context().watch_handle(m_socket.native_handle(), reinterpret_cast<void*>(&operation), detail::watch_type::readable);
	return true;
}


void cppcoro::net::socket_recv_operation_impl::cancel(
	cppcoro::detail::linux_async_operation_base& operation) noexcept
{
	operation.m_ioService->get_io_context().unwatch_handle(m_socket.native_handle());
}

#endif
