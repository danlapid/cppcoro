///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <system_error>

#include <cppcoro/net/socket.hpp>

#include <cppcoro/net/socket_accept_operation.hpp>
#include <cppcoro/net/socket_connect_operation.hpp>
#include <cppcoro/net/socket_disconnect_operation.hpp>
#include <cppcoro/net/socket_recv_operation.hpp>
#include <cppcoro/net/socket_send_operation.hpp>

#include <cppcoro/io_service.hpp>
#include <cppcoro/on_scope_exit.hpp>

#include "socket_helpers.hpp"

#if CPPCORO_OS_WINNT
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <winsock2.h>
# include <ws2tcpip.h>
# include <windows.h>
int get_error() {return ::WSAGetLastError();}
#elif CPPCORO_OS_LINUX
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <netinet/udp.h>
# include <unistd.h>
# include <cstring>
#define closesocket close
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define SD_SEND SHUT_WR
#define SD_RECEIVE SHUT_RD
int get_error() {return errno;}
#endif

namespace
{
	namespace local
	{
#if CPPCORO_OS_WINNT
		cppcoro::net::socket create_socket(
			int addressFamily,
			int socketType,
			int protocol,
			cppcoro::io_service& ioSvc)
		{
			// Enumerate available protocol providers for the specified socket type.

			WSAPROTOCOL_INFO stackInfos[4];
			std::unique_ptr<WSAPROTOCOL_INFO[]> heapInfos;
			WSAPROTOCOL_INFO* selectedProtocolInfo = nullptr;

			{
				INT protocols[] = { protocol, 0 };
				DWORD bufferSize = sizeof(stackInfos);
				WSAPROTOCOL_INFO* infos = stackInfos;

				int protocolCount = ::WSAEnumProtocols(protocols, infos, &bufferSize);
				if (protocolCount == SOCKET_ERROR)
				{
					int errorCode = get_error();
					if (errorCode == WSAENOBUFS)
					{
						DWORD requiredElementCount = bufferSize / sizeof(WSAPROTOCOL_INFO);
						heapInfos = std::make_unique<WSAPROTOCOL_INFO[]>(requiredElementCount);
						bufferSize = requiredElementCount * sizeof(WSAPROTOCOL_INFO);
						infos = heapInfos.get();
						protocolCount = ::WSAEnumProtocols(protocols, infos, &bufferSize);
						if (protocolCount == SOCKET_ERROR)
						{
							errorCode = get_error();
						}
					}

					if (protocolCount == SOCKET_ERROR)
					{
						throw std::system_error(
							errorCode,
							std::system_category(),
							"Error creating socket: WSAEnumProtocols");
					}
				}

				if (protocolCount == 0)
				{
					throw std::system_error(
						std::make_error_code(std::errc::protocol_not_supported));
				}

				for (int i = 0; i < protocolCount; ++i)
				{
					auto& info = infos[i];
					if (info.iAddressFamily == addressFamily && info.iProtocol == protocol && info.iSocketType == socketType)
					{
						selectedProtocolInfo = &info;
						break;
					}
				}

				if (selectedProtocolInfo == nullptr)
				{
					throw std::system_error(
						std::make_error_code(std::errc::address_family_not_supported));
				}
			}

			// WSA_FLAG_NO_HANDLE_INHERIT for SDKs earlier than Windows 7.
			constexpr DWORD flagNoInherit = 0x80;

			const DWORD flags = WSA_FLAG_OVERLAPPED | flagNoInherit;

			const SOCKET socketHandle = ::WSASocket(
				addressFamily, socketType, protocol, selectedProtocolInfo, 0, flags);
			if (socketHandle == INVALID_SOCKET)
			{
				const int errorCode = get_error();
				throw std::system_error(
					errorCode,
					std::system_category(),
					"Error creating socket: WSASocketW");
			}

			auto closeSocketOnFailure = cppcoro::on_scope_failure([&]
			{
				::closesocket(socketHandle);
			});

			// This is needed on operating systems earlier than Windows 7 to prevent
			// socket handles from being inherited. On Windows 7 or later this is
			// redundant as the WSA_FLAG_NO_HANDLE_INHERIT flag passed to creation
			// above causes the socket to be atomically created with this flag cleared.
			if (!::SetHandleInformation((HANDLE)socketHandle, HANDLE_FLAG_INHERIT, 0))
			{
				const DWORD errorCode = ::GetLastError();
				throw std::system_error(
					errorCode,
					std::system_category(),
					"Error creating socket: SetHandleInformation");
			}

			// Associate the socket with the I/O completion port.
			ioSvc.get_io_context().add_handle((HANDLE)socketHandle);

			if (socketType == SOCK_STREAM)
			{
				// Turn off linger so that the destructor doesn't block while closing
				// the socket or silently continue to flush remaining data in the
				// background after ::closesocket() is called, which could fail and
				// we'd never know about it.
				// We expect clients to call Disconnect() or use CloseSend() to cleanly
				// shut-down connections instead.
				BOOL value = TRUE;
				const int result = ::setsockopt(socketHandle,
					SOL_SOCKET,
					SO_DONTLINGER,
					reinterpret_cast<const char*>(&value),
					sizeof(value));
				if (result == SOCKET_ERROR)
				{
					const int errorCode = get_error();
					throw std::system_error(
						errorCode,
						std::system_category(),
						"Error creating socket: setsockopt(SO_DONTLINGER)");
				}
			}

			return cppcoro::net::socket(socketHandle, &ioSvc);
		}
#elif CPPCORO_OS_LINUX
		cppcoro::net::socket create_socket(
			int addressFamily,
			int socketType,
			int protocol,
			cppcoro::io_service& ioSvc)
		{

			const int socketHandle = ::socket(addressFamily, socketType | SOCK_NONBLOCK, protocol);
			if (socketHandle == INVALID_SOCKET)
			{
				const int errorCode = get_error();
				throw std::system_error(
					errorCode,
					std::system_category(),
					"Error creating socket");
			}

			auto closeSocketOnFailure = cppcoro::on_scope_failure([&]
			{
				::closesocket(socketHandle);
			});

			if (socketType == SOCK_STREAM)
			{
				// Turn off linger so that the destructor doesn't block while closing
				// the socket or silently continue to flush remaining data in the
				// background after ::closesocket() is called, which could fail and
				// we'd never know about it.
				// We expect clients to call Disconnect() or use CloseSend() to cleanly
				// shut-down connections instead.
				struct linger value;
				value.l_onoff  = 0;
				const int result = ::setsockopt(socketHandle,
					SOL_SOCKET,
					SO_LINGER,
					reinterpret_cast<const void*>(&value),
					sizeof(value));
				if (result == SOCKET_ERROR)
				{
					const int errorCode = get_error();
					throw std::system_error(
						errorCode,
						std::system_category(),
						"Error creating socket: setsockopt(SO_LINGER)");
				}
			}

			ioSvc.get_io_context().add_handle(socketHandle);
			return cppcoro::net::socket(socketHandle, &ioSvc);
		}
#endif
	}
}

cppcoro::net::socket cppcoro::net::socket::create_tcpv4(io_service& ioSvc)
{
	auto result = local::create_socket(
		AF_INET, SOCK_STREAM, IPPROTO_TCP, ioSvc);
	result.m_localEndPoint = ipv4_endpoint();
	result.m_remoteEndPoint = ipv4_endpoint();
	return result;
}

cppcoro::net::socket cppcoro::net::socket::create_tcpv6(io_service& ioSvc)
{
	auto result = local::create_socket(
		AF_INET6, SOCK_STREAM, IPPROTO_TCP, ioSvc);
	result.m_localEndPoint = ipv6_endpoint();
	result.m_remoteEndPoint = ipv6_endpoint();
	return result;
}

cppcoro::net::socket cppcoro::net::socket::create_udpv4(io_service& ioSvc)
{
	auto result = local::create_socket(
		AF_INET, SOCK_DGRAM, IPPROTO_UDP, ioSvc);
	result.m_localEndPoint = ipv4_endpoint();
	result.m_remoteEndPoint = ipv4_endpoint();
	return result;
}

cppcoro::net::socket cppcoro::net::socket::create_udpv6(io_service& ioSvc)
{
	auto result = local::create_socket(
		AF_INET6, SOCK_DGRAM, IPPROTO_UDP, ioSvc);
	result.m_localEndPoint = ipv6_endpoint();
	result.m_remoteEndPoint = ipv6_endpoint();
	return result;
}

void cppcoro::net::socket::bind(const ip_endpoint& localEndPoint)
{
	sockaddr_storage sockaddrStorage = { 0 };
	sockaddr* sa = reinterpret_cast<sockaddr*>(&sockaddrStorage);
	const int addrLength =
		detail::ip_endpoint_to_sockaddr(localEndPoint, std::ref(sockaddrStorage));

	int result = ::bind(m_handle, sa, addrLength);
	if (result != 0)
	{
		int errorCode = get_error();
		throw std::system_error(
			errorCode,
			std::system_category(),
			"Error binding to endpoint: bind()");
	}

	socklen_t sockaddrLen = sizeof(sockaddrStorage);
	result = ::getsockname(m_handle, sa, &sockaddrLen);
	if (result == 0)
	{
		m_localEndPoint = cppcoro::net::detail::sockaddr_to_ip_endpoint(*sa);
	}
	else
	{
		m_localEndPoint = localEndPoint;
	}
}

void cppcoro::net::socket::listen()
{
	int result = ::listen(m_handle, SOMAXCONN);
	if (result != 0)
	{
		int errorCode = get_error();
		throw std::system_error(
			errorCode,
			std::system_category(),
			"Failed to start listening on bound endpoint: listen");
	}
}

void cppcoro::net::socket::listen(std::uint32_t backlog)
{
	if (backlog > 0x7FFFFFFF)
	{
		backlog = 0x7FFFFFFF;
	}

	int result = ::listen(m_handle, (int)backlog);
	if (result != 0)
	{
		int errorCode = get_error();
		throw std::system_error(
			errorCode,
			std::system_category(),
			"Failed to start listening on bound endpoint: listen");
	}
}

cppcoro::net::socket_accept_operation
cppcoro::net::socket::accept(socket& acceptingSocket) noexcept
{
	return socket_accept_operation{ *this, acceptingSocket, m_ioService};
}

cppcoro::net::socket_accept_operation_cancellable
cppcoro::net::socket::accept(socket& acceptingSocket, cancellation_token ct) noexcept
{
	return socket_accept_operation_cancellable{ *this, acceptingSocket, m_ioService, std::move(ct) };
}

cppcoro::net::socket_connect_operation
cppcoro::net::socket::connect(const ip_endpoint& remoteEndPoint) noexcept
{
	return socket_connect_operation{ *this, remoteEndPoint, m_ioService };
}

cppcoro::net::socket_connect_operation_cancellable
cppcoro::net::socket::connect(const ip_endpoint& remoteEndPoint, cancellation_token ct) noexcept
{
	return socket_connect_operation_cancellable{ *this, remoteEndPoint, m_ioService, std::move(ct) };
}

cppcoro::net::socket_disconnect_operation
cppcoro::net::socket::disconnect() noexcept
{
	return socket_disconnect_operation(*this, m_ioService);
}

cppcoro::net::socket_disconnect_operation_cancellable
cppcoro::net::socket::disconnect(cancellation_token ct) noexcept
{
	return socket_disconnect_operation_cancellable{ *this, m_ioService, std::move(ct) };
}

cppcoro::net::socket_send_operation
cppcoro::net::socket::send(const void* buffer, std::size_t byteCount) noexcept
{
	return socket_send_operation{ *this, buffer, byteCount, m_ioService };
}

cppcoro::net::socket_send_operation_cancellable
cppcoro::net::socket::send(const void* buffer, std::size_t byteCount, cancellation_token ct) noexcept
{
	return socket_send_operation_cancellable{ *this, buffer, byteCount, m_ioService, std::move(ct) };
}

cppcoro::net::socket_recv_operation
cppcoro::net::socket::recv(void* buffer, std::size_t byteCount) noexcept
{
	return socket_recv_operation{ *this, buffer, byteCount, m_ioService };
}

cppcoro::net::socket_recv_operation_cancellable
cppcoro::net::socket::recv(void* buffer, std::size_t byteCount, cancellation_token ct) noexcept
{
	return socket_recv_operation_cancellable{ *this, buffer, byteCount, m_ioService, std::move(ct) };
}

cppcoro::net::socket_recv_from_operation
cppcoro::net::socket::recv_from(void* buffer, std::size_t byteCount) noexcept
{
	return socket_recv_from_operation{ *this, buffer, byteCount, m_ioService };
}

cppcoro::net::socket_recv_from_operation_cancellable
cppcoro::net::socket::recv_from(void* buffer, std::size_t byteCount, cancellation_token ct) noexcept
{
	return socket_recv_from_operation_cancellable{ *this, buffer, byteCount, m_ioService, std::move(ct) };
}

cppcoro::net::socket_send_to_operation
cppcoro::net::socket::send_to(const ip_endpoint& destination, const void* buffer, std::size_t byteCount) noexcept
{
	return socket_send_to_operation{ *this, destination, buffer, byteCount, m_ioService };
}

cppcoro::net::socket_send_to_operation_cancellable
cppcoro::net::socket::send_to(const ip_endpoint& destination, const void* buffer, std::size_t byteCount, cancellation_token ct) noexcept
{
	return socket_send_to_operation_cancellable{ *this, destination, buffer, byteCount, m_ioService, std::move(ct) };
}

void cppcoro::net::socket::close_send()
{
	int result = ::shutdown(m_handle, SD_SEND);
	if (result == SOCKET_ERROR)
	{
		int errorCode = get_error();
		throw std::system_error(
			errorCode,
			std::system_category(),
			"failed to close socket send stream: shutdown(SD_SEND)");
	}
}

void cppcoro::net::socket::close_recv()
{
	int result = ::shutdown(m_handle, SD_RECEIVE);
	if (result == SOCKET_ERROR)
	{
		int errorCode = get_error();
		throw std::system_error(
			errorCode,
			std::system_category(),
			"failed to close socket receive stream: shutdown(SD_RECEIVE)");
	}
}

cppcoro::net::socket::socket(socket&& other) noexcept
	: m_handle(std::exchange(other.m_handle, INVALID_SOCKET))
	, m_ioService(std::move(other.m_ioService))
	, m_localEndPoint(std::move(other.m_localEndPoint))
	, m_remoteEndPoint(std::move(other.m_remoteEndPoint))
{}

cppcoro::net::socket&
cppcoro::net::socket::operator=(socket&& other) noexcept
{
	m_handle = std::exchange(other.m_handle, INVALID_SOCKET);
	m_ioService = std::move(other.m_ioService);
	m_localEndPoint = std::move(other.m_localEndPoint);
	m_remoteEndPoint = std::move(other.m_remoteEndPoint);

	return *this;
}

cppcoro::detail::socket_handle_t duplicate_socket(const cppcoro::detail::socket_handle_t& handle) {
#if CPPCORO_OS_WINNT
	WSAPROTOCOL_INFO wsa_pi;
    WSADuplicateSocket(handle, GetCurrentProcessId(), &wsa_pi);
    return WSASocket(wsa_pi.iAddressFamily, wsa_pi.iSocketType, wsa_pi.iProtocol, &wsa_pi, 0, 0);
#elif CPPCORO_OS_LINUX
	return dup(handle);
#endif
}

cppcoro::net::socket::socket(const socket& other) noexcept
	: m_handle(duplicate_socket(other.m_handle))
	, m_ioService(other.m_ioService)
	, m_localEndPoint(other.m_localEndPoint)
	, m_remoteEndPoint(other.m_remoteEndPoint)
{}

cppcoro::net::socket&
cppcoro::net::socket::operator=(const socket& other) noexcept
{
	m_handle = duplicate_socket(other.m_handle);
	m_ioService = other.m_ioService;
	m_localEndPoint = other.m_localEndPoint;
	m_remoteEndPoint = other.m_remoteEndPoint;

	return *this;
}

cppcoro::net::socket::~socket()
{
	close();
}

int cppcoro::net::socket::close()
{
	if (m_handle != INVALID_SOCKET)
	{
		int res = ::closesocket(m_handle);
		m_handle = INVALID_SOCKET;
		return res;
	}
	return 0;
}

cppcoro::net::socket::socket(
	cppcoro::detail::socket_handle_t handle,
	cppcoro::io_service* ioService) noexcept
	: m_handle(handle)
	, m_ioService(ioService)
{
}
