///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/file.hpp>
#include <cppcoro/io_service.hpp>

#include <system_error>
#include <cassert>

#if CPPCORO_OS_WINNT
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
#elif CPPCORO_OS_LINUX || CPPCORO_OS_DARWIN
# include <fcntl.h>
# include <sys/stat.h>
# include <sys/types.h>
# include <unistd.h>
#endif

cppcoro::file::~file()
{
	m_ioService->get_io_context().remove_handle(m_fileHandle.handle());
}

std::uint64_t cppcoro::file::size() const
{
#if CPPCORO_OS_WINNT
	LARGE_INTEGER size;
	BOOL ok = ::GetFileSizeEx(m_fileHandle.handle(), &size);
	if (!ok)
	{
		DWORD errorCode = ::GetLastError();
		throw std::system_error
		{
			static_cast<int>(errorCode),
			std::system_category(),
			"error getting file size: GetFileSizeEx"
		};
	}

	return size.QuadPart;
#elif CPPCORO_OS_LINUX || CPPCORO_OS_DARWIN
	struct stat sb;
	if (fstat(m_fileHandle.handle(), &sb) < 0)
	{
		throw std::system_error
		{
			errno,
			std::system_category(),
			"error getting file size: fstat"
		};
	}

	return sb.st_size;
#endif
}

cppcoro::file::file(cppcoro::detail::safe_file_handle_t fileHandle, cppcoro::io_service* ioService)
	: m_fileHandle(std::move(fileHandle))
	, m_ioService(ioService)
{
	// Associate with the I/O service's completion port.
	m_ioService->get_io_context().add_handle(m_fileHandle.handle());
}


#if CPPCORO_OS_WINNT
cppcoro::file cppcoro::file::open(
	int fileAccess,
	io_service& ioService,
	const cppcoro::filesystem::path& path,
	file_open_mode openMode,
	file_share_mode shareMode,
	file_buffering_mode bufferingMode)
{
	DWORD flags = FILE_FLAG_OVERLAPPED;
	if ((bufferingMode & file_buffering_mode::random_access) == file_buffering_mode::random_access)
	{
		flags |= FILE_FLAG_RANDOM_ACCESS;
	}
	if ((bufferingMode & file_buffering_mode::sequential) == file_buffering_mode::sequential)
	{
		flags |= FILE_FLAG_SEQUENTIAL_SCAN;
	}
	if ((bufferingMode & file_buffering_mode::write_through) == file_buffering_mode::write_through)
	{
		flags |= FILE_FLAG_WRITE_THROUGH;
	}
	if ((bufferingMode & file_buffering_mode::temporary) == file_buffering_mode::temporary)
	{
		flags |= FILE_ATTRIBUTE_TEMPORARY;
	}
	if ((bufferingMode & file_buffering_mode::unbuffered) == file_buffering_mode::unbuffered)
	{
		flags |= FILE_FLAG_NO_BUFFERING;
	}

	DWORD shareFlags = 0;
	if ((shareMode & file_share_mode::read) == file_share_mode::read)
	{
		shareFlags |= FILE_SHARE_READ;
	}
	if ((shareMode & file_share_mode::write) == file_share_mode::write)
	{
		shareFlags |= FILE_SHARE_WRITE;
	}
	if ((shareMode & file_share_mode::delete_) == file_share_mode::delete_)
	{
		shareFlags |= FILE_SHARE_DELETE;
	}

	DWORD creationDisposition = 0;
	switch (openMode)
	{
	case file_open_mode::create_or_open:
		creationDisposition = OPEN_ALWAYS;
		break;
	case file_open_mode::create_always:
		creationDisposition = CREATE_ALWAYS;
		break;
	case file_open_mode::create_new:
		creationDisposition = CREATE_NEW;
		break;
	case file_open_mode::open_existing:
		creationDisposition = OPEN_EXISTING;
		break;
	case file_open_mode::truncate_existing:
		creationDisposition = TRUNCATE_EXISTING;
		break;
	}

	// Open the file
	detail::safe_file_handle_t fileHandle(
		::CreateFileW(
			path.wstring().c_str(),
			static_cast<detail::win32::dword_t>(fileAccess),
			shareFlags,
			nullptr,
			creationDisposition,
			flags,
			nullptr));
	if (fileHandle.handle() == INVALID_HANDLE_VALUE)
	{
		const DWORD errorCode = ::GetLastError();
		throw std::system_error
		{
			static_cast<int>(errorCode),
			std::system_category(),
			"error opening file: CreateFileW"
		};
	}

	return { std::move(fileHandle), &ioService };
}

#elif CPPCORO_OS_LINUX || CPPCORO_OS_DARWIN

cppcoro::file cppcoro::file::open(
	int fileAccess,
	io_service& ioService,
	const std::filesystem::path& path,
	cppcoro::file_open_mode openMode,
	cppcoro::file_share_mode shareMode,
	cppcoro::file_buffering_mode bufferingMode)
{
	int flags = fileAccess;

	if ((bufferingMode & file_buffering_mode::temporary) == file_buffering_mode::temporary)
	{
		// TODO
	}
	if ((bufferingMode & file_buffering_mode::unbuffered) == file_buffering_mode::unbuffered)
	{
		// TODO
	}

	if ((shareMode & file_share_mode::read) == file_share_mode::read)
	{
		// TODO
	}
	if ((shareMode & file_share_mode::write) == file_share_mode::write)
	{
		// TODO
	}
	if ((shareMode & file_share_mode::delete_) == file_share_mode::delete_)
	{
		// TODO
	}

	switch (openMode)
	{
	case file_open_mode::create_or_open:
		flags |= O_CREAT;
		break;
	case file_open_mode::create_always:
		flags |= O_CREAT | O_TRUNC;
		break;
	case file_open_mode::create_new:
		flags |= O_EXCL;
		break;
	case file_open_mode::open_existing:
		// Default.
		break;
	case file_open_mode::truncate_existing:
		flags |= O_TRUNC;
		break;
	}

	cppcoro::detail::safe_file_handle_t fd(
		::open(path.c_str(), flags | O_NONBLOCK, S_IRWXU | S_IRWXG));
	if (fd.handle() < 0)
	{
		throw std::system_error
		{
			errno,
			std::system_category(),
			"error opening file: open"
		};
	}

	//posix_fadvise(fd.get(), 0, 0, advice);
	return { std::move(fd), &ioService };
}
#endif
