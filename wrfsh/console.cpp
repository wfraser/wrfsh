#include <string>
#include <memory>
#include <vector>
#include <streambuf>
#include <iostream>

#include "common.h"
#include "console.h"

using namespace std;

Console* Console::make()
{
#ifdef _MSC_VER
    return new Console_Win32();
#else
    return new Console_Posix();
#endif
}

void Console::prompt()
{
    write_output("wrfsh> ", CharAttr::FG_Green | CharAttr::FG_Bold);
}
