#pragma once

class stream_ex
{
public:

#ifdef _MSC_VER
    typedef HANDLE native_handle;
#else
    typedef int native_handle;
#endif

    ~stream_ex();

    bool operator==(const std::ios& stream);
    bool operator==(native_handle handle);
    bool operator==(const stream_ex& other);

    native_handle get_native_handle() const;

protected:

    enum class kind
    {
        IOS,        // Created from a std::ios
        Native      // Created from a native handle
    };

    stream_ex(std::ios* stream);
    stream_ex(native_handle handle, std::ios_base::openmode mode);
    stream_ex(path_t path, std::ios_base::openmode mode);

    kind m_kind;
    std::ios* m_stream;
    native_handle m_handle;

#ifdef __GNUC__
    std::basic_filebuf<char>* m_filebuf;
#endif

private:
    void open_native_handle(std::ios_base::openmode mode);
};

class istream_ex : public stream_ex, public std::istream
{
public:
    istream_ex(std::istream* stream);
    istream_ex(native_handle handle);
    istream_ex(path_t path);
};

class ostream_ex : public stream_ex, public std::ostream
{
public:
    ostream_ex(std::ostream* stream);
    ostream_ex(native_handle handle);
    ostream_ex(path_t path);
};
