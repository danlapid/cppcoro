///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/net/socket_disconnect_operation.hpp>
#include <cppcoro/net/socket.hpp>
#include <cppcoro/io_service.hpp>

#include "socket_helpers.hpp"

#include <system_error>

#if CPPCORO_OS_WINNT
# include <winsock2.h>
# include <ws2tcpip.h>
# include <mswsock.h>
# include <windows.h>

bool cppcoro::net::socket_disconnect_operation_impl::try_start(
	cppcoro::detail::async_operation_base& operation) noexcept
{
	operation.m_ioState.m_handle = reinterpret_cast<HANDLE>(m_socket.native_handle());
	// Lookup the address of the DisconnectEx function pointer for this socket.
	LPFN_DISCONNECTEX disconnectExPtr;
	{
		GUID disconnectExGuid = WSAID_DISCONNECTEX;
		DWORD byteCount = 0;
		const int result = ::WSAIoctl(
			m_socket.native_handle(),
			SIO_GET_EXTENSION_FUNCTION_POINTER,
			static_cast<void*>(&disconnectExGuid),
			sizeof(disconnectExGuid),
			static_cast<void*>(&disconnectExPtr),
			sizeof(disconnectExPtr),
			&byteCount,
			nullptr,
			nullptr);
		if (result == SOCKET_ERROR)
		{
			operation.m_ioState.m_errorCode = static_cast<DWORD>(::WSAGetLastError());
			return false;
		}
	}

	// Need to add TF_REUSE_SOCKET to these flags if we want to allow reusing
	// a socket for subsequent connections once the disconnect operation
	// completes.
	const DWORD flags = 0;

	const BOOL ok = disconnectExPtr(
		m_socket.native_handle(),
		operation.m_ioState.get_overlapped(),
		flags,
		0);
	if (!ok)
	{
		const int errorCode = ::WSAGetLastError();
		if (errorCode != ERROR_IO_PENDING)
		{
			// Failed synchronously.
			operation.m_ioState.m_errorCode = static_cast<DWORD>(errorCode);
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

	return true;
}

void cppcoro::net::socket_disconnect_operation_impl::get_result(
	cppcoro::detail::async_operation_base& operation)
{
	if (operation.m_ioState.m_errorCode != ERROR_SUCCESS)
	{
		if (operation.m_ioState.m_errorCode == ERROR_OPERATION_ABORTED)
		{
			throw operation_cancelled{};
		}

		throw std::system_error{
			static_cast<int>(operation.m_ioState.m_errorCode),
			std::system_category(),
			"Disconnect operation failed: DisconnectEx"
		};
	}
}
#elif CPPCORO_OS_LINUX
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <netinet/udp.h>

bool cppcoro::net::socket_disconnect_operation_impl::try_start(
	cppcoro::detail::async_operation_base& operation) noexcept
{
	operation.m_ioState.m_fd = m_socket.native_handle();
	operation.m_ioState.m_completeFunc = [&]() {
		return m_socket.close();
	};
	operation.m_ioState.m_ioService->get_io_context().watch_handle(m_socket.native_handle(), reinterpret_cast<void*>(&operation), cppcoro::detail::watch_type::writable);
	return true;
}

void cppcoro::net::socket_disconnect_operation_impl::get_result(
	cppcoro::detail::async_operation_base& operation)
{
	if (operation.m_ioState.m_res < 0)
	{
		if (operation.m_ioState.m_res == -ECANCELED)
		{
			throw operation_cancelled{};
		}

		throw std::system_error{
			static_cast<int>(-operation.m_ioState.m_res),
			std::system_category(),
			"Disconnect operation failed: disconnect"
		};
	}
}
#endif
