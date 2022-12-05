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

void cppcoro::detail::win32::safe_handle::close() noexcept
{
	if (m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE)
	{
		::CloseHandle(m_handle);
		m_handle = nullptr;
	}
}

cppcoro::detail::win32::safe_handle cppcoro::detail::win32::create_auto_reset_event()
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

	return cppcoro::detail::win32::safe_handle{ eventHandle };
}

cppcoro::detail::win32::safe_handle cppcoro::detail::win32::create_waitable_timer_event()
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

	return cppcoro::detail::win32::safe_handle{ handle };
}
