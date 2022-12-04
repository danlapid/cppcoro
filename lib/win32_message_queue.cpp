///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Microsoft
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#include <cppcoro/detail/message_queue.hpp>
#include <cassert>
#include <cstring>
#include <system_error>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <windows.h>

namespace cppcoro
{
	namespace detail
	{
		safe_file_handle_t create_io_completion_port(std::uint32_t concurrencyHint)
		{
			HANDLE handle =
				::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, concurrencyHint);
			if (handle == NULL)
			{
				DWORD errorCode = ::GetLastError();
				throw std::system_error{ static_cast<int>(errorCode),
										 std::system_category(),
										 "Error creating io_service: CreateIoCompletionPort" };
			}

			return safe_file_handle_t{ handle };
		}

		message_queue::message_queue(std::uint32_t concurrencyHint)
			: m_pollfd(safe_file_handle_t{ create_io_completion_port(concurrencyHint) })
		{
			const WORD requestedVersion = MAKEWORD(2, 2);
			WSADATA winsockData;
			const int result = ::WSAStartup(requestedVersion, &winsockData);
			if (result == SOCKET_ERROR)
			{
				const int errorCode = ::WSAGetLastError();
				throw std::system_error(
					errorCode, std::system_category(), "Error initialsing winsock: WSAStartup");
			}
		}

		message_queue::~message_queue()
		{
			// TODO: Should we be checking return-code here?
			// Don't want to throw from the destructor, so perhaps just log an error?
			(void)::WSACleanup();
		}

		void message_queue::add_handle(file_handle_t handle)
		{
			const HANDLE result =
				::CreateIoCompletionPort(handle, m_pollfd.handle(), ULONG_PTR(0), DWORD(0));
			if (result == nullptr)
			{
				const DWORD errorCode = ::GetLastError();
				throw std::system_error(
					static_cast<int>(errorCode),
					std::system_category(),
					"Error creating socket: CreateIoCompletionPort");
			}
		}
		void message_queue::remove_handle(file_handle_t handle)
		{
		}

		void message_queue::watch_handle(file_handle_t handle, void* cb, watch_type events)
		{
		}

		void message_queue::unwatch_handle(file_handle_t handle)
		{
		}

		bool message_queue::enqueue_message(message msg)
		{
			return ::PostQueuedCompletionStatus(
				m_pollfd.handle(), 0, reinterpret_cast<ULONG_PTR>(msg.data), nullptr);
		}

		bool message_queue::dequeue_message(message& msg, bool wait)
		{
			const DWORD timeout = wait ? INFINITE : 0;
			DWORD numberOfBytesTransferred = 0;
			ULONG_PTR completionKey = 0;
			LPOVERLAPPED overlapped = nullptr;
			BOOL ok = ::GetQueuedCompletionStatus(
				m_pollfd.handle(), &numberOfBytesTransferred, &completionKey, &overlapped, timeout);
			if (overlapped != nullptr)
			{
				msg.type = message_type::CALLBACK_TYPE;
				msg.data = reinterpret_cast<void*>(overlapped);
				return true;
			}
			else if (ok)
			{
				if (completionKey != 0)
				{
					msg.type = message_type::RESUME_TYPE;
					msg.data = reinterpret_cast<void*>(completionKey);
					return true;
				}
			}
			else
			{
				const DWORD errorCode = ::GetLastError();
				if (errorCode == WAIT_TIMEOUT)
				{
					return false;
				}

				throw std::system_error{
					static_cast<int>(errorCode),
					std::system_category(),
					"Error retrieving item from io_service queue: GetQueuedCompletionStatus"
				};
			}
			return false;
		}
	}  // namespace detail
}  // namespace cppcoro
