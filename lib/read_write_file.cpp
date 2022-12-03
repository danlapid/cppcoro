///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/read_write_file.hpp>

#if CPPCORO_OS_WINNT
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
#elif CPPCORO_OS_LINUX
#include <fcntl.h>
#endif

cppcoro::read_write_file cppcoro::read_write_file::open(
	io_service& ioService,
	const cppcoro::filesystem::path& path,
	file_open_mode openMode,
	file_share_mode shareMode,
	file_buffering_mode bufferingMode)
{
	return read_write_file(file::open(
#if CPPCORO_OS_WINNT
		GENERIC_READ | GENERIC_WRITE,
#elif CPPCORO_OS_LINUX
		O_RDWR,
#endif
		ioService,
		path,
		openMode,
		shareMode,
		bufferingMode));
}

cppcoro::read_write_file::read_write_file(
	detail::safe_file_handle&& fileHandle) noexcept
	: file(std::move(fileHandle))
	, readable_file(detail::safe_file_handle{})
	, writable_file(detail::safe_file_handle{})
{
}
