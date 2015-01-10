#pragma once

#ifdef _MSC_VER

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <comdef.h>

#define snprintf(buf, n, format, ...)  _snprintf_s(buf, n, n, format, __VA_ARGS__)

#else
// Posix

#include <unistd.h>

#define countof(_) (sizeof(_) / sizeof(*_))

#endif

#ifdef __CYGWIN__

// Cygwin's GCC is missing some things.

#include <sstream>
#include <memory>
#include <stdlib.h> // for the definition of atoi (it lacks std::atoi)

namespace std
{
    template <typename T>
    inline string to_string(const T& x)
    {
        stringstream ss;
        ss << x;
        return ss.str();
    }

    template <typename T, typename... ArgTypes>
    inline
    typename enable_if<!is_array<T>::value,
        unique_ptr<T>>::type
    make_unique(ArgTypes&&... args)
    {
        return unique_ptr<T>(new T(forward<ArgTypes>(args)...));
    }

    template <typename T>
    inline
    typename enable_if<is_array<T>::value && extent<T>::value == 0,
        unique_ptr<T>>::type
    make_unique(size_t size)
    {
        typedef typename remove_extent<T>::type T2;
        return unique_ptr<T>(new T2[size]);
    }

    template <typename T, typename... ArgTypes>
    inline
    typename enable_if<extent<T>::value != 0,
        void>::type
    make_unique(ArgTypes&&...) = delete;
}

#endif
