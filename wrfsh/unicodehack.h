#pragma once

#ifdef _MSC_VER

// Forward declare enough to get string and wstring.
// (because <string> brings in <fstream> eventually)
namespace std
{
    template <typename T> struct char_traits;
    template <typename T> class allocator;
    template <typename A, typename B, typename C> class basic_string;

    typedef basic_string<char, char_traits<char>, allocator<char>> string;
    typedef basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t>> wstring;
}

struct _iobuf;

namespace std
{
    // Define a new basic_[io]fstream with the char* and string constructors deleted.
    //
    // This makes sure that all code will use the correct conversion to wchar_t,
    // because in windows, char* and string are encoded in the local code page, and
    // Unicode is only supported by the wchar_t* and wstring functions, which are
    // utf-16.)

    // TODO: add redefinitions of the other constructors that forward to the actual
    // class. This is needed because if any overridden functions are defined in this
    // template, _ALL_ overrides need to be defined and explicitly forwareded. They
    // are _NOT_ inherited from the base template class.

#define UNICODEHACK(className, openMode) \
    template <typename _Elem, typename _Traits> class basic_##className; \
    \
    template <typename _Elem, typename _Traits> \
    class basic_##className##_unicodehack : public basic_##className<_Elem, _Traits> \
    { \
    public: \
        typedef basic_##className<_Elem, _Traits> T; \
        \
        basic_##className##_unicodehack() : T() \
        {} \
        \
        explicit basic_##className##_unicodehack( \
            const char *filename_utf8, \
            int mode = openMode \
            ) = delete; \
        \
        explicit basic_##className##_unicodehack( \
            const string& filename_utf8, \
            int mode = openMode \
            ) = delete; \
        \
        explicit basic_##className##_unicodehack( \
            const wchar_t *filename_utf16, \
            int mode = openMode \
            ) : T(filename_utf16, mode) \
        {} \
        \
        explicit basic_##className##_unicodehack( \
            const wstring filename_utf16, \
            int mode = openMode \
            ) : T(filename_utf16, mode) \
        {} \
        \
        explicit basic_##className##_unicodehack( \
            _iobuf* file \
            ) : T(file) \
        {} \
        \
        void open( \
            const char *filename, \
            int mode = openMode \
            ) = delete; \
        \
        void open( \
            const string& filename, \
            int mode = openMode \
            ) = delete; \
        \
        void open( \
            const wchar_t *filename, \
            int mode = openMode \
            ) \
        { \
            T::open(filename, mode); \
        } \
        \
        void open( \
            const wstring& filename, \
            int mode = openMode \
            ) \
        { \
            T::open(filename, mode); \
        } \
    }; \
    \
    typedef basic_##className##_unicodehack<char, char_traits<char>> className

    // These should be ios::in, ios::out, etc.,
    // but they're defined in <ios>, which brings in <fstream>, which we can't have yet.

    UNICODEHACK(ifstream, 1); // ios::in
    UNICODEHACK(ofstream, 2); // ios::out
    UNICODEHACK(fstream, 3); // ios::in | ios::out

#undef UNICODEHACK
}

// Rename so that <iosfwd> makes differently-named typedefs from ours.
#define ifstream ifstream_stl;
#define ofstream ofstream_stl;
#define fstream  fstream_stl;

// Pull in the full template definition for the basic_* classes.
#include <fstream>

// Restore the typedefs to the unicodehack versions.
#undef ifstream
#undef ofstream
#undef fstream

// Define two utility functions for converting utf8 <-> utf16.
#include <codecvt>
inline std::wstring Widen(std::string utf8)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> convert;
    return convert.from_bytes(utf8);
}
inline std::string Narrow(std::wstring utf16)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> convert;
    return convert.to_bytes(utf16);
}

// Depending on the compiler version, define a converter from utf8 to whatever
// type it is the system uses natively.
#if _MSC_VER >= 1900 // Dev14

// This is awesome because path::string_type is automatically correct for the
// target OS.
#include <filesystem>
typedef std::tr2::sys::path::string_type native_string_t;
inline native_string_t ospath(std::string s)
{
    return std::tr2::sys::u8path(s).native();
}

#else // Dev12

typedef std::wstring native_string_t;
inline native_string_t ospath(std::string s)
{
    return Widen(s);
}

#endif // Dev14

#else // _MSC_VER

// Assuming Posix

#include <string>
typedef std::string native_string_t;

/*
path_t ospath(std::string s)
{
return s;
}
*/
#define ospath(_s) ((native_string_t)(_s))

#define Narrow(_s) (_s)

#endif
