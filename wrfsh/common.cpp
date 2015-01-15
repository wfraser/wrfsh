#include "unicodehack.h"

#include <iostream>
#include <string>

#include "common.h"

using namespace std;

string get_current_working_directory(ostream& error_output)
{
    static const char* s_err = "error getting current directory: ";

#ifdef _MSC_VER
    DWORD requiredSize = GetCurrentDirectoryW(0, nullptr);
    wstring buf;
    buf.resize(requiredSize - 1);

    if (GetCurrentDirectoryW(requiredSize, &buf[0]) != (requiredSize - 1))
    {
        DWORD error = GetLastError();
        error_output << s_err << Narrow(_com_error(HRESULT_FROM_WIN32(error)).ErrorMessage());
        return ".";
    }
    return Narrow(buf);
#else
    //TODO: test this
    char* dir = get_current_dir_name();
    if (dir == nullptr)
    {
        error_output << s_err << strerror(errno) << endl;
        return ".";
    }

    string dir_str = dir;
    free(dir);
    return dir_str;
#endif
}

#ifdef _MSC_VER

int compare_string_nocase(const std::wstring& a, const std::wstring& b, int n)
{
    switch (CompareStringEx(
        LOCALE_NAME_INVARIANT,
        NORM_IGNORECASE,
        a.c_str(),
        n,
        b.c_str(),
        n,
        nullptr,
        nullptr,
        0))
    {
    case CSTR_LESS_THAN:
        return -1;
    case CSTR_EQUAL:
        return 0;
    case CSTR_GREATER_THAN:
        return 1;
    default:
        throw new exception();
    }
}

int compare_string_nocase(const std::wstring& a, const std::wstring& b)
{
    switch (CompareStringEx(
        LOCALE_NAME_INVARIANT,
        NORM_IGNORECASE,
        a.c_str(),
        static_cast<int>(a.size()),
        b.c_str(),
        static_cast<int>(b.size()),
        nullptr,
        nullptr,
        0))
    {
    case CSTR_LESS_THAN:
        return -1;
    case CSTR_EQUAL:
        return 0;
    case CSTR_GREATER_THAN:
        return 1;
    default:
        throw new exception();
    }
}

int compare_string_nocase(const std::string& a, const std::string& b, int n)
{
    wstring wa = Widen(a);
    wstring wb = Widen(b);
    return compare_string_nocase(wa, wb, n);
}

int compare_string_nocase(const std::string& a, const std::string& b)
{
    wstring wa = Widen(a);
    wstring wb = Widen(b);
    return compare_string_nocase(wa, wb);
}

#endif