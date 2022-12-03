///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/write_only_file.hpp>

#if CPPCORO_OS_WINNT
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
#elif CPPCORO_OS_LINUX
#include <fcntl.h>
#endif

cppcoro::write_only_file cppcoro::write_only_file::open(
	io_service& ioService,
	const cppcoro::filesystem::path& path,
	file_open_mode openMode,
	file_share_mode shareMode,
	file_buffering_mode bufferingMode)
{
	return write_only_file(file::open(
#if CPPCORO_OS_WINNT
		GENERIC_WRITE,
#elif CPPCORO_OS_LINUX
		O_WRONLY,
#endif
		ioService,
		path,
		openMode,
		shareMode,
		bufferingMode));
}

cppcoro::write_only_file::write_only_file(
	detail::safe_file_handle&& fileHandle) noexcept
	: file(std::move(fileHandle))
	, writable_file(detail::safe_file_handle{})
{
}
