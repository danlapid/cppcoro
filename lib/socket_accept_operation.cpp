///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/net/socket_accept_operation.hpp>
#include <cppcoro/net/socket.hpp>
#include <cppcoro/io_service.hpp>

#include "socket_helpers.hpp"

#include <system_error>

#if CPPCORO_OS_WINNT
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <winsock2.h>
# include <ws2tcpip.h>
# include <mswsock.h>
# include <windows.h>

// TODO: Eliminate duplication of implementation between socket_accept_operation
// and socket_accept_operation_cancellable.

bool cppcoro::net::socket_accept_operation_impl::try_start(
	cppcoro::detail::async_operation_base& operation) noexcept
{
	operation.m_handle = reinterpret_cast<HANDLE>(m_listeningSocket.native_handle());
	static_assert(
		(sizeof(m_addressBuffer) / 2) >= (16 + sizeof(SOCKADDR_IN)) &&
		(sizeof(m_addressBuffer) / 2) >= (16 + sizeof(SOCKADDR_IN6)),
		"AcceptEx requires address buffer to be at least 16 bytes more than largest address.");

	DWORD bytesReceived = 0;
	BOOL ok = ::AcceptEx(
		m_listeningSocket.native_handle(),
		m_acceptingSocket.native_handle(),
		m_addressBuffer,
		0,
		sizeof(m_addressBuffer) / 2,
		sizeof(m_addressBuffer) / 2,
		&bytesReceived,
		operation.get_overlapped());
	if (!ok)
	{
		int errorCode = ::WSAGetLastError();
		if (errorCode != ERROR_IO_PENDING)
		{
			operation.m_errorCode = static_cast<DWORD>(errorCode);
			return false;
		}
	}
	operation.m_completeFunc = [&]() {
		cppcoro::detail::win32::dword_t numberOfBytesTransferred = 0;
		cppcoro::detail::win32::dword_t flags = 0;
		cppcoro::detail::win32::bool_t ok = WSAGetOverlappedResult(
			m_listeningSocket.native_handle(),
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

	return true;
}

void cppcoro::net::socket_accept_operation_impl::get_result(
	cppcoro::detail::async_operation_base& operation)
{
	if (operation.m_errorCode != ERROR_SUCCESS)
	{
		throw std::system_error{
			static_cast<int>(operation.m_errorCode),
			std::system_category(),
			"Accepting a connection failed: AcceptEx"
		};
	}

	sockaddr* localSockaddr = nullptr;
	sockaddr* remoteSockaddr = nullptr;

	INT localSockaddrLength;
	INT remoteSockaddrLength;

	::GetAcceptExSockaddrs(
		m_addressBuffer,
		0,
		sizeof(m_addressBuffer) / 2,
		sizeof(m_addressBuffer) / 2,
		&localSockaddr,
		&localSockaddrLength,
		&remoteSockaddr,
		&remoteSockaddrLength);

	m_acceptingSocket.m_localEndPoint =
		detail::sockaddr_to_ip_endpoint(*localSockaddr);

	m_acceptingSocket.m_remoteEndPoint =
		detail::sockaddr_to_ip_endpoint(*remoteSockaddr);

	{
		// Need to set SO_UPDATE_ACCEPT_CONTEXT after the accept completes
		// to ensure that ::shutdown() and ::setsockopt() calls work on the
		// accepted socket.
		SOCKET listenSocket = m_listeningSocket.native_handle();
		const int result = ::setsockopt(
			m_acceptingSocket.native_handle(),
			SOL_SOCKET,
			SO_UPDATE_ACCEPT_CONTEXT,
			(const char*)&listenSocket,
			sizeof(SOCKET));
		if (result == SOCKET_ERROR)
		{
			const int errorCode = ::WSAGetLastError();
			throw std::system_error{
				errorCode,
				std::system_category(),
				"Socket accept operation failed: setsockopt(SO_UPDATE_ACCEPT_CONTEXT)"
			};
		}
	}
}
#elif CPPCORO_OS_LINUX || CPPCORO_OS_DARWIN
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <netinet/udp.h>
bool cppcoro::net::socket_accept_operation_impl::try_start(
	cppcoro::detail::async_operation_base& operation) noexcept
{
	operation.m_fd = m_listeningSocket.native_handle();
	static_assert(
		(sizeof(m_addressBuffer) / 2) >= (16 + sizeof(sockaddr_in)) &&
		(sizeof(m_addressBuffer) / 2) >= (16 + sizeof(sockaddr_in6)),
		"AcceptEx requires address buffer to be at least 16 bytes more than largest address.");

	operation.m_completeFunc = [&]() {
		socklen_t len = sizeof(m_addressBuffer) / 2;
		return accept(m_listeningSocket.native_handle(), reinterpret_cast<sockaddr*>(m_addressBuffer), &len);
	};
	operation.m_ioService->get_io_context().watch_handle(m_listeningSocket.native_handle(), reinterpret_cast<void*>(&operation), cppcoro::detail::watch_type::readable);
	return true;
}

void cppcoro::net::socket_accept_operation_impl::get_result(
	cppcoro::detail::async_operation_base& operation)
{
	if (operation.m_res < 0)
	{
		throw std::system_error{
			static_cast<int>(-operation.m_res),
			std::system_category(),
			"Accepting a connection failed: accept"
		};
	}

	m_acceptingSocket = socket(operation.m_res, m_acceptingSocket.m_ioService);
	sockaddr* remoteSockaddr = reinterpret_cast<sockaddr*>(m_addressBuffer);
	sockaddr* localSockaddr = reinterpret_cast<sockaddr*>(m_addressBuffer + sizeof(m_addressBuffer)/2);

	socklen_t len = sizeof(m_addressBuffer) / 2;
	getsockname(m_acceptingSocket.native_handle(), localSockaddr, &len);

	m_acceptingSocket.m_localEndPoint =
		detail::sockaddr_to_ip_endpoint(*localSockaddr);

	m_acceptingSocket.m_remoteEndPoint =
		detail::sockaddr_to_ip_endpoint(*remoteSockaddr);
}

#endif
