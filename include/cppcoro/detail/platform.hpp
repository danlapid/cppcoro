///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_PLATFORM_HPP_INCLUDED
#define CPPCORO_DETAIL_PLATFORM_HPP_INCLUDED

#if CPPCORO_OS_WINNT
# include <cppcoro/detail/win32.hpp>
# include <cppcoro/detail/win32_overlapped_operation.hpp>
#elif CPPCORO_OS_LINUX
# include <cppcoro/detail/linux.hpp>
# include <cppcoro/detail/linux_async_operation.hpp>
#endif


namespace cppcoro
{
	namespace detail
	{
#if CPPCORO_OS_WINNT
		using async_operation_base = win32_overlapped_operation_base;
// using define because of friend class cannot use a templated class over using
#		define async_operation win32_overlapped_operation
#		define async_operation_cancellable win32_overlapped_operation_cancellable
		using file_handle_t = win32::handle_t;
		using safe_file_handle_t = win32::safe_handle;
		using socket_handle_t = win32::socket_t;
		using io_context_t = win32::handle_t;
#elif CPPCORO_OS_LINUX
		using async_operation_base = linux_async_operation_base;
// using define because of friend class cannot use a templated class over using
#		define async_operation linux_async_operation
#		define async_operation_cancellable linux_async_operation_cancellable
		using file_handle_t = linux::fd_t;
		using safe_file_handle_t = linux::safe_fd;
		using socket_handle_t = linux::fd_t;
		using io_context_t = linux::message_queue*;
#endif
	struct safe_file_handle
	{
		safe_file_handle_t fileHandle;
		io_context_t ctx;
	};
    }
}
#endif
