#include "unicodehack.h"

#include <iostream>
#include <string>
#include <memory>
#include <vector>

#include "common.h"
#include "console.h"

using namespace std;

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

Console_Win32::Console_Win32()
    : m_streambuf(new Console_streambuf(this))
    , m_ostream(m_streambuf.get())
{
    m_inputHandle = GetStdHandle(STD_INPUT_HANDLE);
    m_outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    CHKERR(SetConsoleMode(m_inputHandle, ENABLE_QUICK_EDIT_MODE | ENABLE_WINDOW_INPUT));

    GetWindowInfo();

    m_currentInputLineIdx = 0;
}

void Console_Win32::GetWindowInfo()
{
    CONSOLE_SCREEN_BUFFER_INFO info = {};
    CHKERR(GetConsoleScreenBufferInfo(m_outputHandle, &info));
    m_windowSize = { info.srWindow.Right, info.srWindow.Bottom };
    m_cursorPos = info.dwCursorPosition;
}

void Console_Win32::EchoChar(wchar_t c, WORD attrs)
{
    wstring str(1, c);
    EchoString(str, attrs);
}

void Console_Win32::EchoString(const wstring& s, WORD attrs)
{
    DWORD num_written = 0;
    CHKERR(WriteConsoleW(m_outputHandle, s.c_str(), static_cast<DWORD>(s.length()), &num_written, nullptr));

    if (attrs != 0)
    {
        vector<WORD> buf;
        buf.resize(s.size(), attrs);
        CHKERR(WriteConsoleOutputAttribute(m_outputHandle, buf.data(), num_written, m_cursorPos, &num_written));
    }

    GetWindowInfo();
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

void Console_Win32::AdvanceCursorPos(int n)
{
    IncrementWrap<SHORT>(m_cursorPos.X, m_cursorPos.Y, static_cast<SHORT>(n), m_windowSize.X);
    CHKERR(SetConsoleCursorPosition(m_outputHandle, m_cursorPos));
}

string Console_Win32::GetInput()
{
    wstring line;

    for (;;)
    {
        INPUT_RECORD input;
        DWORD records_read = 0;
        CHKERR(ReadConsoleInputW(m_inputHandle, &input, 1, &records_read));

        if (input.EventType == KEY_EVENT)
        {
            KEY_EVENT_RECORD& rec = input.Event.KeyEvent;

            if (rec.uChar.UnicodeChar == L'\b')
            {
                if (rec.bKeyDown && line.length() > 0)
                {
                    AdvanceCursorPos(-1);
                    EchoChar(L' ');
                    AdvanceCursorPos(-1);
                    line.pop_back();
                }
                continue;
            }

            if (rec.bKeyDown)
            {
                switch (rec.wVirtualKeyCode)
                {
                case VK_DOWN:
                case VK_UP:
                case VK_LEFT:
                case VK_RIGHT:
                    //TODO: scroll through history
                    break;
                }
            }

            if (rec.uChar.UnicodeChar != L'\0')
            {
                // Basic characters

                if (rec.uChar.UnicodeChar == '\r')
                    rec.uChar.UnicodeChar = '\n';

                if (rec.bKeyDown/* || !m_lastKeyInputDown*/)
                {
                    EchoChar(rec.uChar.UnicodeChar);
                    line.push_back(rec.uChar.UnicodeChar);
                    m_lastKeyInputDown = true;
                }

                if (!rec.bKeyDown)
                {
                    m_lastKeyInputDown = false;

                    if (rec.wVirtualKeyCode == VK_RETURN)
                    {
                        line[line.length() - 1] = L'\n';    // replace the '\r'

                        m_inputLines.push_back(line);
                        m_currentInputLineIdx++;

                        return Narrow(line);
                    }

                }
            }
        }
        else if (input.EventType == WINDOW_BUFFER_SIZE_EVENT)
        {
            m_windowSize = input.Event.WindowBufferSizeEvent.dwSize;
        }
    }
}

void Console_Win32::WriteOutput(const string& s, CharAttr attrs)
{
    EchoString(Widen(s), static_cast<WORD>(attrs));
}

ostream& Console_Win32::ostream()
{
    return m_ostream;
}