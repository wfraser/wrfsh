#include "unicodehack.h"

#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <assert.h>

#include "common.h"
#include "console.h"

using namespace std;

Console_Win32* Console_Win32::s_pInstance = nullptr;

void chkerr(BOOL b, const char* what, const char* file, int line)
{
    if (!b)
    {
        cerr << what << " failed! Win32 error: " << GetLastError()
            << " at " << file << ":" << line << endl;
        throw new exception();
    }
}

void chkerr(DWORD dw, const char* what, const char* file, int line)
{
    chkerr(dw == 0, what, file, line);
}

#define CHKERR(_ex) chkerr(_ex, #_ex, __FILE__, __LINE__)

BOOL WINAPI Console_Win32::ctrl_handler(DWORD dwCtrlType)
{
    Console_Win32* instance = s_pInstance;
    assert(instance != nullptr);
    if (instance == nullptr)
    {
        return FALSE;
    }

    switch (dwCtrlType)
    {
    case CTRL_C_EVENT:
        {
            int lineIdx = static_cast<int>(instance->m_currentInputLineIdx);
            if (lineIdx == instance->m_inputLines.size() - 1)
            {
                // Current line is the last line -- hasn't been entered yet.
                // Clear the current line.
                instance->clear_current_display_line();
                instance->m_inputLines[instance->m_currentInputLineIdx].clear();
            }
            else
            {
                // Current line is one previously entered.
                // Switch to the empty line.
                instance->new_empty_line();
            }
        }
        return TRUE;
    default:
        OutputDebugStringA("unhandled ctrl-sequence\n");
        return FALSE;
    }
}

Console_Win32::Console_Win32()
    : m_streambuf(new Console_streambuf(this))
    , m_ostream(m_streambuf.get())
{
    m_inputHandle = GetStdHandle(STD_INPUT_HANDLE);
    m_outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    CHKERR(SetConsoleMode(m_inputHandle, ENABLE_QUICK_EDIT_MODE | ENABLE_WINDOW_INPUT | ENABLE_PROCESSED_INPUT));

    get_window_info();

    m_currentInputLineIdx = 0;

    assert(s_pInstance == nullptr);
    s_pInstance = this;

    // FIXME: this doesn't work for some reason.
    CHKERR(SetConsoleCtrlHandler(&Console_Win32::ctrl_handler, true));
}

Console_Win32::~Console_Win32()
{
    // TODO: set console mode back to whatever the default is.
    CHKERR(SetConsoleCtrlHandler(nullptr, false));
    s_pInstance = nullptr;
}

void Console_Win32::get_window_info()
{
    CONSOLE_SCREEN_BUFFER_INFO info = {};
    CHKERR(GetConsoleScreenBufferInfo(m_outputHandle, &info));
    m_windowSize = { info.srWindow.Right, info.srWindow.Bottom };
    m_cursorPos = info.dwCursorPosition;
}

void Console_Win32::echo_char(wchar_t c, CharAttr attrs)
{
    DWORD num_written;
    CHKERR(WriteConsoleW(m_outputHandle, &c, 1, &num_written, nullptr));

    if (attrs != CharAttr::Default)
    {
        WORD w = static_cast<WORD>(attrs);
        CHKERR(WriteConsoleOutputAttribute(m_outputHandle, &w, 1, m_cursorPos, &num_written));
    }

    get_window_info();
}

void Console_Win32::echo_string(const wstring& s, CharAttr attrs)
{
    DWORD num_written = 0;
    CHKERR(WriteConsoleW(m_outputHandle, s.c_str(), static_cast<DWORD>(s.length()), &num_written, nullptr));

    if (attrs != CharAttr::Default)
    {
        vector<WORD> buf;
        buf.resize(s.size(), static_cast<WORD>(attrs));
        CHKERR(WriteConsoleOutputAttribute(m_outputHandle, buf.data(), num_written, m_cursorPos, &num_written));
    }

    get_window_info();
}

template <typename T>
void IncrementWrap(T &x, T &y, T n, T row_size)
{
    x += n;

    T rows = x / row_size;
    T remainder = x % row_size;

    if (rows != 0 && n < 0)
    {
        rows -= 1;
        remainder = row_size + remainder;
    }

    if (rows != 0)
    {
        x = remainder;
        y += rows;
    }
};

void Console_Win32::advance_cursor_pos(int n)
{
    IncrementWrap<SHORT>(m_cursorPos.X, m_cursorPos.Y, static_cast<SHORT>(n), m_windowSize.X);
    CHKERR(SetConsoleCursorPosition(m_outputHandle, m_cursorPos));
}

Console::Input Console_Win32::get_input_char()
{
    Input console_input = {};

    for (;;)
    {
        INPUT_RECORD input;
        DWORD records_read = 0;
        CHKERR(ReadConsoleInputW(m_inputHandle, &input, 1, &records_read));

        if (input.EventType == KEY_EVENT)
        {
            KEY_EVENT_RECORD& rec = input.Event.KeyEvent;
            
            if ((rec.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0)
            {
                // Don't process ctrl-key events.
                continue;
            }

            if (!rec.bKeyDown)
            {
                // Don't process key-up events; only key-down events;
                continue;
            }

            switch (rec.wVirtualKeyCode)
            {
            case VK_UP:
            case VK_DOWN:
            case VK_LEFT:
            case VK_RIGHT:
            case VK_DELETE:
            case VK_BACK:
            case VK_RETURN:
                console_input.type = Input::Type::Special;
                console_input.special = static_cast<Input::Special>(rec.wVirtualKeyCode);
                break;

            case VK_SHIFT:
            case VK_MENU:   // alt
                continue;

            default:
                if (rec.uChar.UnicodeChar == L'\0')
                    continue;
                console_input.type = Input::Type::Character;
                console_input.character = rec.uChar.UnicodeChar;
            }

            return console_input;
        }
        else if (input.EventType == WINDOW_BUFFER_SIZE_EVENT)
        {
            m_windowSize = input.Event.WindowBufferSizeEvent.dwSize;
        }
    } // for (;;)
}

void Console_Win32::write_output(const string& s, CharAttr attrs)
{
    echo_string(Widen(s), attrs);
}

ostream& Console_Win32::ostream()
{
    return m_ostream;
}
