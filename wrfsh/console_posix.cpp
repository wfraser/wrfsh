#include "unicodehack.h"

#include <iostream>
#include <string>
#include <memory>
#include <vector>

#include <termios.h>

#include "common.h"
#include "console.h"

using namespace std;

struct Console_Posix::Details
{
    termios savedTermios;
};

Console_Posix::Console_Posix()
    : m_streambuf(new Console_streambuf(this))
    , m_ostream(m_streambuf.get())
{
    m_details = new Details;
    
    struct termios tp;
    tcgetattr(STDIN_FILENO, &tp);
    m_details->savedTermios = tp;

    cfmakeraw(&tp);

    // One change: we want "\n" to be sent as "\r\n". The rest of the code assumes it works this way.
    tp.c_oflag |= ONLCR|OPOST;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tp);
}

Console_Posix::~Console_Posix()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &(m_details->savedTermios));

    delete m_details;
}

void Console_Posix::write_output(const native_string_t& s, CharAttr attrs)
{
    //TODO
    echo_string(s, attrs);
}

void Console_Posix::advance_cursor_pos(int n)
{
    char str[32];
    char direction = 'C';
    if (n < 0)
    {
        n *= -1;
        direction = 'D';
    }

    if (n != 0)
    {
        int num = snprintf(str, countof(str), "\033[%d%c", n, direction);
        write(STDOUT_FILENO, str, num);
    }
}

ostream& Console_Posix::ostream()
{
    return m_ostream;
}

bool Console_Posix::vt_escape(Console::Input* input)
{
    char c;
    string buf;

    for (;;)
    {
        if (read(STDOUT_FILENO, &c, 1) < 1)
        {
            input->type = Input::Type::Special;
            input->special = Input::Special::Eof;
            return true;
        }

        // TODO: handle VT100 escape codes for arrow keys
        input->type = Input::Type::Character;
        input->character = static_cast<char>(c);
        return true;
    }
}

Console::Input Console_Posix::get_input_char()
{
    for (;;)
    {
        Input input = {};
        char c;
        ssize_t num_read = read(STDOUT_FILENO, &c, 1);
        if (num_read <= 0 || c == 4 /* ASCII EOT */)
        {
            input.type = Input::Type::Special;
            input.special = Input::Special::Eof;
        }
        else
        {
            //printf("got %d\n", c);
            input.type = Input::Type::Special;
            switch (c)
            {
            case '\r':
            case '\n':
                input.special = Input::Special::Return;
                break;
            case 27: // ASCII ESC
                if (!vt_escape(&input))
                    continue;
                break;
            case 127: // ASCII DEL
                printf("backspace\n");
                input.special = Input::Special::Backspace;
                break;
            default:
                input.type = Input::Type::Character;
                input.character = static_cast<char>(c);
            }
        }
        return input;
    }
}

void Console_Posix::echo_char(char c, Console::CharAttr)
{
    ssize_t n = write(STDOUT_FILENO, &c, 1);
    if (n != 1)
        perror("echo_char write");
}

void Console_Posix::echo_string(const string& s, Console::CharAttr)
{
    ssize_t n = write(STDOUT_FILENO, s.c_str(), s.size());
    if (n != static_cast<ssize_t>(s.size()))
        perror("echo_string write");
}
