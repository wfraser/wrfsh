#include "unicodehack.h"
#include <iostream>
#include <fstream>

#include <assert.h>

#ifdef _MSC_VER
#include <io.h>
#else
#include <fcntl.h>
    #ifdef __GNUC__
    #include <ext/stdio_filebuf.h>
    #endif
#endif

#include "common.h"
#include "stream_ex.h"

stream_ex::stream_ex(std::ios* stream)
    : m_kind(kind::IOS)
    , m_stream(stream)
    , m_handle()
{
}

stream_ex::stream_ex(native_handle handle, std::ios_base::openmode mode)
    : m_kind(kind::Native)
    , m_stream(nullptr)
    , m_handle(handle)
{
    open_native_handle(mode);
}

stream_ex::stream_ex(path_t path, std::ios_base::openmode mode)
    : m_kind(kind::Native)
    , m_stream(nullptr)
#ifdef _MSC_VER
    , m_handle(INVALID_HANDLE_VALUE)
#else
    , m_handle(-1)
#endif
{
#ifdef _MSC_VER
    DWORD access = 0;
    if (mode & std::ios_base::in)
    {
        access |= GENERIC_READ;
    }
    if (mode & std::ios_base::out)
    {
        access |= GENERIC_WRITE;
    }

    DWORD creationDisposition = OPEN_ALWAYS;
    if (mode & std::ios_base::trunc)
    {
        creationDisposition |= TRUNCATE_EXISTING;
    }

    m_handle = CreateFileW(path.c_str(), access, FILE_SHARE_READ, nullptr, creationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);

#else
    // UNTESTED!

    mode_t imode = 0;

    std::ios_base::openmode rwmode = mode & (std::ios_base::in | std::ios_base::out);
    if (rwmode == std::ios_base::in)
    {
        imode = O_RDONLY;
    }
    else if (rwmode == std::ios_base::out)
    {
        imode = O_WRONLY;
    }
    else
    {
        imode = O_RDWR;
    }

    int flags = O_CREAT;
    if (mode & std::ios_base::trunc)
    {
        flags |= O_TRUNC;
    }
    if (mode & std::ios_base::app || mode & std::ios_base::ate)
    {
        flags |= O_APPEND;
    }

    m_handle = open(path.c_str(), flags, imode);
#endif

    open_native_handle(mode);
}

void stream_ex::open_native_handle(std::ios_base::openmode mode)
{
#ifdef _MSC_VER
    int fd = _open_osfhandle(reinterpret_cast<intptr_t>(m_handle), 0);
    if (fd == -1)
    {
        throw new std::exception("bad file descriptor from _open_osfhandle");
    }

    const char *modestr;
    if (mode == std::ios_base::in)
    {
        modestr = "r";
    }
    else if (mode == std::ios_base::out)
    {
        modestr = "w";
    }
    else
    {
        // Should this be an error?
        modestr = "rw";
    }

    FILE* file = _fdopen(fd, modestr);
    if (file == nullptr)
    {
        throw new std::exception("null pointer from _fdopen");
    }

    m_stream = new std::fstream(file);
#else
#ifdef __GNUC__
    // UNTESTED!
    m_filebuf = new __gnu_cxx::stdio_filebuf<char>(m_handle, mode);
    m_stream = new std::iostream(m_filebuf);
#else
#error no stream_ex support for your compiler :(
#endif
#endif
}

stream_ex::~stream_ex()
{
    if (m_kind == kind::Native)
    {
        delete m_stream;

#ifdef __GNUC__
        if (m_filebuf != nullptr)
        {
            delete m_filebuf;
        }
#endif
   }
}

bool stream_ex::operator==(const std::ios & stream)
{
    if (m_kind == kind::IOS)
    {
        return (m_stream == &stream);
    }
    return false;
}

bool stream_ex::operator==(native_handle handle)
{
    if (m_kind == kind::Native)
    {
        return (m_handle == handle);
    }
    return false;
}

bool stream_ex::operator==(const stream_ex & other)
{
    switch (m_kind)
    {
    case kind::IOS:
        return (m_stream == other.m_stream);
    case kind::Native:
        return (m_handle == other.m_handle);
    default:
        assert(false);
        return false;
    }
}

stream_ex::native_handle stream_ex::get_native_handle() const
{
    if (m_kind == kind::Native)
    {
        return m_handle;
    }
    else
    {
#ifdef _MSC_VER
        return INVALID_HANDLE_VALUE;
#else
        return -1;
#endif
    }
}

istream_ex::istream_ex(std::istream* stream)
    : stream_ex(stream)
    , std::istream(m_stream->rdbuf())
{
}

istream_ex::istream_ex(native_handle handle)
    : stream_ex(handle, std::ios_base::in)
    , std::istream(m_stream->rdbuf())
{
}

istream_ex::istream_ex(path_t path)
    : stream_ex(path, std::ios_base::in)
    , std::istream(m_stream->rdbuf())
{
}

ostream_ex::ostream_ex(std::ostream* stream)
    : stream_ex(stream)
    , std::ostream(m_stream->rdbuf())
{
}

ostream_ex::ostream_ex(native_handle handle)
    : stream_ex(handle, std::ios_base::out)
    , std::ostream(m_stream->rdbuf())
{
}

ostream_ex::ostream_ex(path_t path)
    : stream_ex(path, std::ios_base::out)
    , std::ostream(m_stream->rdbuf())
{
}
