///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_WIN32_HPP_INCLUDED
#define CPPCORO_DETAIL_WIN32_HPP_INCLUDED

#include <cppcoro/config.hpp>

#if !CPPCORO_OS_WINNT
# error <cppcoro/detail/win32.hpp> is only supported on the Windows platform.
#endif

#include <utility>
#include <functional>
#include <cstdint>

struct _OVERLAPPED;

namespace cppcoro
{
	class io_service;
	namespace detail
	{
		class async_operation_base;
		namespace win32
		{
			using handle_t = void*;
			using ulongptr_t = std::uintptr_t;
			using longptr_t = std::intptr_t;
			using dword_t = unsigned long;
			using socket_t = std::uintptr_t;
			using ulong_t = unsigned long;
			using bool_t = int;

#if CPPCORO_COMPILER_MSVC
# pragma warning(push)
# pragma warning(disable : 4201) // Non-standard anonymous struct/union
#endif

			/// Structure needs to correspond exactly to the builtin
			/// _OVERLAPPED structure from Windows.h.
			struct overlapped
			{
				ulongptr_t Internal;
				ulongptr_t InternalHigh;
				union
				{
					struct
					{
						dword_t Offset;
						dword_t OffsetHigh;
					};
					void* Pointer;
				};
				handle_t hEvent;
			};

#if CPPCORO_COMPILER_MSVC
# pragma warning(pop)
#endif

			struct wsabuf
			{
				constexpr wsabuf() noexcept
					: len(0)
					, buf(nullptr)
				{}

				constexpr wsabuf(void* ptr, std::size_t size)
					: len(size <= ulong_t(-1) ? ulong_t(size) : ulong_t(-1))
					, buf(static_cast<char*>(ptr))
				{}

				ulong_t len;
				char* buf;
			};

			struct io_state : overlapped
			{

				io_state(io_service* ioService) noexcept
					: m_ioService(ioService)
					, m_handle(nullptr)
					, m_errorCode(0)
					, m_numberOfBytesTransferred(0)
					, overlapped{0}
				{}

				_OVERLAPPED* get_overlapped() noexcept;
				std::size_t get_result();
				void on_operation_completed();
				void cancel(void* operation) noexcept;

				io_service* m_ioService;
				async_operation_base* operation;
				handle_t m_handle;
				dword_t m_errorCode;
				dword_t m_numberOfBytesTransferred;
				std::function<int64_t()> m_completeFunc;
			};

			class safe_handle
			{
			public:

				safe_handle()
					: m_handle(nullptr)
				{}

				explicit safe_handle(handle_t handle)
					: m_handle(handle)
				{}

				safe_handle(const safe_handle& other) = delete;

				safe_handle(safe_handle&& other) noexcept
					: m_handle(other.m_handle)
				{
					other.m_handle = nullptr;
				}

				~safe_handle()
				{
					close();
				}

				safe_handle& operator=(safe_handle handle) noexcept
				{
					swap(handle);
					return *this;
				}

				constexpr handle_t handle() const { return m_handle; }

				/// Calls CloseHandle() and sets the handle to NULL.
				void close() noexcept;

				void swap(safe_handle& other) noexcept
				{
					std::swap(m_handle, other.m_handle);
				}

				bool operator==(const safe_handle& other) const
				{
					return m_handle == other.m_handle;
				}

				bool operator!=(const safe_handle& other) const
				{
					return m_handle != other.m_handle;
				}

				bool operator==(handle_t handle) const
				{
					return m_handle == handle;
				}

				bool operator!=(handle_t handle) const
				{
					return m_handle != handle;
				}

			private:

				handle_t m_handle;

			};

			safe_handle create_waitable_timer_event();
			safe_handle create_auto_reset_event();
		}
	}
}

#endif
