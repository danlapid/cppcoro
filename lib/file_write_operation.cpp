///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/file_write_operation.hpp>

#if CPPCORO_OS_WINNT
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <Windows.h>

bool cppcoro::file_write_operation_impl::try_start(
	cppcoro::detail::win32_overlapped_operation_base& operation) noexcept
{
	const DWORD numberOfBytesToWrite =
		m_byteCount <= 0xFFFFFFFF ?
		static_cast<DWORD>(m_byteCount) : DWORD(0xFFFFFFFF);

	operation.get_overlapped()->Offset = static_cast<cppcoro::detail::win32::dword_t>(m_offset);
	operation.get_overlapped()->OffsetHigh = static_cast<cppcoro::detail::win32::dword_t>(m_offset >> 32);
	DWORD numberOfBytesWritten = 0;
	BOOL ok = ::WriteFile(
		m_fileHandle,
		m_buffer,
		numberOfBytesToWrite,
		&numberOfBytesWritten,
		operation.get_overlapped());
	const DWORD errorCode = ok ? ERROR_SUCCESS : ::GetLastError();
	if (errorCode != ERROR_IO_PENDING)
	{
		// Completed synchronously.
		//
		// We are assuming that the file-handle has been set to the
		// mode where synchronous completions do not post a completion
		// event to the I/O completion port and thus can return without
		// suspending here.

		operation.m_errorCode = errorCode;
		operation.m_numberOfBytesTransferred = numberOfBytesWritten;

		return false;
	}

	return true;
}

void cppcoro::file_write_operation_impl::cancel(
	cppcoro::detail::win32_overlapped_operation_base& operation) noexcept
{
	(void)::CancelIoEx(m_fileHandle, operation.get_overlapped());
}

#elif CPPCORO_OS_LINUX

bool cppcoro::file_write_operation_impl::try_start(
	cppcoro::detail::linux_async_operation_base& operation) noexcept
{
	auto seek_res = lseek(m_fileHandle, m_offset, SEEK_SET);
	if (seek_res < 0) {
		operation.m_res = -errno;
		return false;
	}
	operation.m_completeFunc = [=]() {
		int res = write(m_fileHandle, m_buffer, m_byteCount);
		operation.m_ctx->remove_fd_watch(m_fileHandle);
		return res;
	};
	operation.m_ctx->add_fd_watch(m_fileHandle, reinterpret_cast<void*>(&operation), EPOLLOUT);
	return true;
}

void cppcoro::file_write_operation_impl::cancel(
	cppcoro::detail::linux_async_operation_base& operation) noexcept
{
	operation.m_ctx->remove_fd_watch(m_fileHandle);
}
#endif
