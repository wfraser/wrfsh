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