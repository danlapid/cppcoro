///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_PLATFORM_HPP_INCLUDED
#define CPPCORO_DETAIL_PLATFORM_HPP_INCLUDED

#include <cppcoro/config.hpp>
#if CPPCORO_OS_WINNT
# include <cppcoro/detail/win32.hpp>
#elif CPPCORO_OS_LINUX
# include <cppcoro/detail/linux.hpp>
#endif


namespace cppcoro
{
	namespace detail
	{
#if CPPCORO_OS_WINNT
		using file_handle_t = win32::handle_t;
		using safe_file_handle_t = win32::safe_handle;
		using socket_handle_t = win32::socket_t;
		using io_state = win32::io_state;
#elif CPPCORO_OS_LINUX
		using file_handle_t = linux::fd_t;
		using safe_file_handle_t = linux::safe_fd;
		using socket_handle_t = linux::fd_t;
		using io_state = linux::io_state;
#endif
    }
}
#endif
