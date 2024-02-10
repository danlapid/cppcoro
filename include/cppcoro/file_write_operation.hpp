///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_FILE_WRITE_OPERATION_HPP_INCLUDED
#define CPPCORO_FILE_WRITE_OPERATION_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/cancellation_registration.hpp>
#include <cppcoro/cancellation_token.hpp>

#include <atomic>
#include <optional>

#include <cppcoro/detail/platform.hpp>
#include <cppcoro/detail/async_operation.hpp>

namespace cppcoro
{
 	class file_write_operation_impl
 	{
 	public:

 		file_write_operation_impl(
 			detail::file_handle_t fileHandle,
 			std::uint64_t fileOffset,
 			const void* buffer,
 			std::size_t byteCount) noexcept
 			: m_fileHandle(fileHandle)
 			, m_offset(fileOffset)
 			, m_buffer(buffer)
 			, m_byteCount(byteCount)
 		{}

 		bool try_start(cppcoro::detail::async_operation_base& operation) noexcept;
 		void cancel(cppcoro::detail::async_operation_base& operation) noexcept;

 	private:

 		detail::file_handle_t m_fileHandle;
 		std::uint64_t m_offset;
 		const void* m_buffer;
 		std::size_t m_byteCount;

 	};

 	class file_write_operation
 		: public cppcoro::detail::async_operation<file_write_operation>
 	{
 	public:

 		file_write_operation(
 			detail::file_handle_t fileHandle,
 			std::uint64_t fileOffset,
 			const void* buffer,
 			std::size_t byteCount,
			io_service* ioService) noexcept
 			: cppcoro::detail::async_operation<file_write_operation>(ioService)
 			, m_impl(fileHandle, fileOffset, buffer, byteCount)
 		{}

 	private:

 		friend class cppcoro::detail::async_operation<file_write_operation>;

 		bool try_start() noexcept { return m_impl.try_start(*this); }

 		file_write_operation_impl m_impl;

 	};

 	class file_write_operation_cancellable
 		: public cppcoro::detail::async_operation_cancellable<file_write_operation_cancellable>
 	{
 	public:

 		file_write_operation_cancellable(
 			detail::file_handle_t fileHandle,
 			std::uint64_t fileOffset,
 			const void* buffer,
 			std::size_t byteCount,
			io_service* ioService,
 			cancellation_token&& ct) noexcept
 			: cppcoro::detail::async_operation_cancellable<file_write_operation_cancellable>(
 				ioService, std::move(ct))
 			, m_impl(fileHandle, fileOffset, buffer, byteCount)
 		{}

 	private:

 		friend class cppcoro::detail::async_operation_cancellable<file_write_operation_cancellable>;

 		bool try_start() noexcept { return m_impl.try_start(*this); }

 		file_write_operation_impl m_impl;

 	};
}


#endif
