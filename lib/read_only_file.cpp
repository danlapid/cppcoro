///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/read_only_file.hpp>

#if CPPCORO_OS_WINNT
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
#elif CPPCORO_OS_LINUX
#include <fcntl.h>
#endif

cppcoro::read_only_file cppcoro::read_only_file::open(
	io_service& ioService,
	const cppcoro::filesystem::path& path,
	file_share_mode shareMode,
	file_buffering_mode bufferingMode)
{
	return read_only_file(file::open(
#if CPPCORO_OS_WINNT
		GENERIC_READ,
#elif CPPCORO_OS_LINUX
		O_RDONLY,
#endif
		ioService,
		path,
		file_open_mode::open_existing,
		shareMode,
		bufferingMode));
}

cppcoro::read_only_file::read_only_file(file&& other) noexcept
	: file(std::move(other))
	, readable_file(detail::safe_file_handle_t{}, NULL)
{
}
