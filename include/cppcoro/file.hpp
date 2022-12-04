///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_FILE_HPP_INCLUDED
#define CPPCORO_FILE_HPP_INCLUDED

#include <cppcoro/config.hpp>

#include <cppcoro/filesystem.hpp>
#include <cppcoro/file_open_mode.hpp>
#include <cppcoro/file_share_mode.hpp>
#include <cppcoro/file_buffering_mode.hpp>

#include <cppcoro/detail/platform.hpp>


namespace cppcoro
{
	class io_service;

	class file
	{
	public:
 		file(const file& other) noexcept = delete;
 		file& operator=(const file& other) noexcept = delete;
 		file(file&& other) noexcept = default;
 		file& operator=(file&& other) noexcept = default;
		virtual ~file();
		/// Get the size of the file in bytes.
		std::uint64_t size() const;

	protected:

 		static file open(
 			int fileAccess,
 			io_service& ioService,
 			const std::filesystem::path& path,
 			file_open_mode openMode,
 			file_share_mode shareMode,
 			file_buffering_mode bufferingMode);

		file(detail::safe_file_handle_t fileHandle, io_service* ioService);

 		detail::safe_file_handle_t m_fileHandle;
		io_service* m_ioService;
	};
}

#endif
