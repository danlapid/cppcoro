///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/detail/win32.hpp>

#include <system_error>
#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>


namespace cppcoro
{
	namespace detail
	{
		namespace win32
		{
			void safe_handle::close() noexcept
			{
				if (m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE)
				{
					::CloseHandle(m_handle);
					m_handle = nullptr;
				}
			}

			safe_handle create_auto_reset_event()
			{
				HANDLE eventHandle = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
				if (eventHandle == NULL)
				{
					const DWORD errorCode = ::GetLastError();
					throw std::system_error
					{
						static_cast<int>(errorCode),
						std::system_category(),
						"Error creating manual reset event: CreateEventW"
					};
				}

				return safe_handle{ eventHandle };
			}

			safe_handle create_waitable_timer_event()
			{
				const BOOL isManualReset = FALSE;
				HANDLE handle = ::CreateWaitableTimerW(nullptr, isManualReset, nullptr);
				if (handle == nullptr)
				{
					const DWORD errorCode = ::GetLastError();
					throw std::system_error
					{
						static_cast<int>(errorCode),
						std::system_category()
					};
				}

				return safe_handle{ handle };
			}

			_OVERLAPPED* io_state::get_overlapped() noexcept
			{
				return reinterpret_cast<_OVERLAPPED*>(
					static_cast<detail::win32::overlapped*>(this));
			}

			std::size_t io_state::get_result()
			{
				if (m_errorCode != 0)
				{
					throw std::system_error{
						static_cast<int>(m_errorCode),
						std::system_category()
					};
				}

				return m_numberOfBytesTransferred;
			}

			void io_state::on_operation_completed_base()
			{
				int64_t res = m_completeFunc();
				if (res < 0) {
					m_errorCode = -res;
					m_numberOfBytesTransferred = 0;
				} else {
					m_errorCode = 0;
					m_numberOfBytesTransferred = res;
				}
			}

			void io_state::cancel() noexcept
			{
				m_errorCode = ERROR_OPERATION_ABORTED;
#if CPPCORO_OS_WINNT >= 0x600
				(void)::CancelIoEx(m_handle, get_overlapped());
#else
				(void)::CancelIo(m_handle);
#endif
			}
		}
	}
}
