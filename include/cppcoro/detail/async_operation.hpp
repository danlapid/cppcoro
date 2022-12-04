///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_ASYNC_OPERATION_HPP_INCLUDED
#define CPPCORO_DETAIL_ASYNC_OPERATION_HPP_INCLUDED

#include <cppcoro/config.hpp>
#if CPPCORO_OS_WINNT
# include <cppcoro/detail/win32_overlapped_operation.hpp>
#elif CPPCORO_OS_LINUX
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
#elif CPPCORO_OS_LINUX
		using async_operation_base = linux_async_operation_base;
// using define because of friend class cannot use a templated class over using
#		define async_operation linux_async_operation
#		define async_operation_cancellable linux_async_operation_cancellable
#endif
    }
}
#endif
