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

		file(file&& other) noexcept = default;

		virtual ~file();

		/// Get the size of the file in bytes.
		std::uint64_t size() const;

	protected:
 		file(detail::safe_file_handle&& fileHandle) noexcept;

 		static detail::safe_file_handle open(
 			int fileAccess,
 			io_service& ioService,
 			const std::filesystem::path& path,
 			file_open_mode openMode,
 			file_share_mode shareMode,
 			file_buffering_mode bufferingMode);

 		detail::safe_file_handle m_fileHandle;
	};
}

#endif
