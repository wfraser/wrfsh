#include <string>
#include <memory>
#include <vector>
#include <streambuf>

#include "common.h"
#include "console.h"

using namespace std;

Console* Console::Make()
{
#ifdef _MSC_VER
    return new Console_Win32();
#else
    return new Console_Posix();
#endif
}

void Console::Prompt()
{
    WriteOutput("wrfsh> ", CharAttr::FG_Green | CharAttr::FG_Bold);
}