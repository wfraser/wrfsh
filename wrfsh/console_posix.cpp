#include <iostream>
#include <string>
#include <memory>

#include "common.h"
#include "console.h"

using namespace std;

Console_Posix::Console_Posix()
    : m_streambuf(new Console_streambuf(this))
    , m_ostream(m_streambuf.get())
{
    //TODO
}

Console_Posix::~Console_Posix()
{
    // Empty
}

string Console_Posix::get_input()
{
    //TODO
    return "echo \"this isn't working yet.\"\nexit\n";
}

void Console_Posix::write_output(const string& s, CharAttr attrs)
{
    //TODO
    cout << s;
}

void Console_Posix::advance_cursor_pos(int n)
{
    //TODO
}

ostream& Console_Posix::ostream()
{
    return m_ostream;
}
