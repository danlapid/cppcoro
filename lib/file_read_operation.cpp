///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/file_read_operation.hpp>
#include <cppcoro/io_service.hpp>

#if CPPCORO_OS_WINNT
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>

bool cppcoro::file_read_operation_impl::try_start(
	cppcoro::detail::async_operation_base& operation) noexcept
{
	operation.m_handle = m_fileHandle;
	const DWORD numberOfBytesToRead =
		m_byteCount <= 0xFFFFFFFF ?
		static_cast<DWORD>(m_byteCount) : DWORD(0xFFFFFFFF);

	operation.get_overlapped()->Offset = static_cast<cppcoro::detail::win32::dword_t>(m_offset);
	operation.get_overlapped()->OffsetHigh = static_cast<cppcoro::detail::win32::dword_t>(m_offset >> 32);
	DWORD numberOfBytesRead = 0;
	BOOL ok = ::ReadFile(
		m_fileHandle,
		m_buffer,
		numberOfBytesToRead,
		&numberOfBytesRead,
		operation.get_overlapped());
	const DWORD errorCode = ::GetLastError();
	if (!ok && errorCode != ERROR_IO_PENDING)
	{
		// Completed synchronously.
		//
		// We are assuming that the file-handle has been set to the
		// mode where synchronous completions do not post a completion
		// event to the I/O completion port and thus can return without
		// suspending here.

		operation.m_errorCode = errorCode;
		operation.m_numberOfBytesTransferred = numberOfBytesRead;

		return false;
	}
	operation.m_completeFunc = [&]() {
		detail::win32::dword_t numberOfBytesTransferred = 0;
		detail::win32::bool_t ok = GetOverlappedResult(
			m_fileHandle,
			operation.get_overlapped(),
			&numberOfBytesTransferred,
			0
		);
		if (ok) {
			return std::make_tuple(static_cast<detail::win32::dword_t>(ERROR_SUCCESS), numberOfBytesTransferred);
		} else {
			return std::make_tuple(GetLastError(), numberOfBytesTransferred);
		}
	};

	return true;
}

#elif CPPCORO_OS_LINUX || CPPCORO_OS_DARWIN
#include <unistd.h>

bool cppcoro::file_read_operation_impl::try_start(
	cppcoro::detail::async_operation_base& operation) noexcept
{
	operation.m_fd = m_fileHandle;
	auto seek_res = lseek(m_fileHandle, m_offset, SEEK_SET);
	if (seek_res < 0) {
		operation.m_res = -errno;
		return false;
	}
	operation.m_completeFunc = [&]() {
		return read(m_fileHandle, m_buffer, m_byteCount);
	};
	operation.m_ioService->get_io_context().watch_handle(m_fileHandle, reinterpret_cast<void*>(&operation), detail::watch_type::readable);
	return true;
}
#endif
