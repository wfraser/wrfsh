#include "unicodehack.h"

#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <deque>

#include <termio.h>

#include "common.h"
#include "console.h"

using namespace std;

struct Console_Posix::Details
{
    termios savedTermios;
    struct winsize winsize;
    struct
    {
        int x, y;
    } cursor;
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

bool Console_Posix::vt_escape()
{
    Input input = {};
    input.type = Input::Type::Special;

    string buf;

    char c;
    int arg[16] = {};
    size_t arg_num = 0;

    for (;;)
    {
        if (read(STDIN_FILENO, &c, 1) < 1)
        {
            input.special = Input::Special::Eof;
            m_pendingInputs.push_back(input);
            return true;
        }

        buf.push_back(c);

        switch (c)
        {
        case '[':
            break;

        case 'A':
            input.special = Input::Special::Up;
            goto direction_key;
        case 'B':
            input.special = Input::Special::Down;
            goto direction_key;
        case 'C':
            input.special = Input::Special::Right;
            goto direction_key;
        case 'D':
            input.special = Input::Special::Left;
            goto direction_key;
        
        direction_key:
            if (arg[0] == 0)
                arg[0] = 1;
            for (int i = 0; i < arg[0]; i++)
            {
                m_pendingInputs.push_back(input);
            }
            return true;

        case '~':
            

        case ';':
            arg_num++;
            if (arg_num == countof(arg))
            {
                printf("too many arguments to CSI sequence\n");
                return false;
            }
            break;

        default:
            if (c >= '0' && c <= '9')
            {
                arg[arg_num] *= 10;
                arg[arg_num] += (c - '0');
            }
            else
            {
                printf("bad escape: ");
                for (size_t i = 0; i < buf.size(); i++)
                {
                    printf("%02x ", buf[i]);
                }
                printf("\n");
                return false;
            }
            break;
        }

        // TODO: handle VT100 escape codes for arrow keys
        //input->type = Input::Type::Character;
        //input->character = static_cast<char>(c);
        //return true;
    }
}

Console::Input Console_Posix::get_input_char()
{
    for (;;)
    {
        Input input = {};

        if (m_pendingInputs.size() > 0)
        {
            input = m_pendingInputs.front();
            m_pendingInputs.pop_front();
            return input;
        }

        char c;
        ssize_t num_read = read(STDIN_FILENO, &c, 1);
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
                vt_escape();
                continue;
            case 127: // ASCII DEL
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

void SetColor(Console::CharAttr attrs)
{
    if (attrs == Console::CharAttr::Default)
        return; // don't bother

    string code = "\033[3x;4x";
    const size_t fgPos = 3;
    const size_t bgPos = 6;

    switch (static_cast<int>(attrs) & 0x7)
    {
    case 0: code[fgPos] = '0'; break; // black
    case 1: code[fgPos] = '4'; break; // blue
    case 2: code[fgPos] = '2'; break; // green
    case 3: code[fgPos] = '6'; break; // blue + green = cyan
    case 4: code[fgPos] = '1'; break; // red
    case 5: code[fgPos] = '5'; break; // red + blue = magenta
    case 6: code[fgPos] = '3'; break; // red + green = yellow
    case 7: code[fgPos] = '7'; break; // red + green + blue = white
    }

    switch (static_cast<int>(attrs) & 0x70)
    {
    case 0x00: code[bgPos] = '0'; break; // black
    case 0x10: code[bgPos] = '4'; break; // blue
    case 0x20: code[bgPos] = '2'; break; // green
    case 0x30: code[bgPos] = '6'; break; // blue + green = cyan
    case 0x40: code[bgPos] = '1'; break; // red
    case 0x50: code[bgPos] = '5'; break; // red + blue = magenta
    case 0x60: code[bgPos] = '3'; break; // red + green = yellow
    case 0x70: code[bgPos] = '7'; break; // red + green + blue = white
    }

    if ((attrs & Console::CharAttr::FG_Bold) != Console::CharAttr::None)
        code.append(";1");
    if ((attrs & Console::CharAttr::BG_Bold) != Console::CharAttr::None)
        code.append(";2");
    if ((attrs & Console::CharAttr::Reverse) != Console::CharAttr::None)
        code.append(";7");
    if ((attrs & Console::CharAttr::Underline) != Console::CharAttr::None)
        code.append(";4");

    code.push_back('m');
    write(STDOUT_FILENO, code.c_str(), code.size());
}

void ResetColor()
{
    const char str[] = "\033[0m";
    write(STDOUT_FILENO, str, countof(str) - 1);
}

void Console_Posix::echo_char(char c, Console::CharAttr attrs)
{
    SetColor(attrs);
    ssize_t n = write(STDOUT_FILENO, &c, 1);
    if (n != 1)
        perror("echo_char write");
    ResetColor();
}

void Console_Posix::echo_string(const string& s, Console::CharAttr attrs)
{
    SetColor(attrs);
    ssize_t n = write(STDOUT_FILENO, s.c_str(), s.size());
    if (n != static_cast<ssize_t>(s.size()))
        perror("echo_string write");
    ResetColor();
}

void Console_Posix::get_window_info()
{
    if (-1 == ioctl(STDOUT_FILENO, TIOCGWINSZ, &(m_details->winsize)))
    {
        perror("ioctl(TIOGCWINSZ)");
        abort();
        return;
    }

    const char s[] = "\033[6n";
    if (write(STDOUT_FILENO, s, countof(s) - 1) < static_cast<ssize_t>(countof(s) - 1))
    {
        perror("write DSR");
        abort();
        return;
    }

    enum class States
    {
        Esc,
        Bracket,
        Y,
        X
    };
    States state = States::Esc;

    int x = 0;
    int y = 0;
    char c;
    for (size_t i = 0; i < 32; i++)
    {
        if (read(STDIN_FILENO, &c, 1) < 1)
        {
            printf("!0!");
            abort();
            break;
        }
     
        switch (state)
        {
        case States::Esc:
            if (c != '\033')
            {
                printf("1:%d!", c);
                abort();
                return;
            }
            else
            {
                state = States::Bracket;
            }
            break;
        case States::Bracket:
            if (c != '[')
            {
                printf("!2:%d!", c);
                abort();
                return;
            }
            else
            {
                state = States::Y;
            }
            break;
        case States::Y:
            if (c == ';')
            {
                m_details->cursor.y = y;
                state = States::X;
            }
            else
            {
                y *= 10;
                y += (c - '0');
            }
            break;
        case States::X:
            if (c == 'R')
            {
                m_details->cursor.x = x;
                return;
            }
            else
            {
                x *= 10;
                x += (c - '0');
            }
            break;
        }
    }
}
