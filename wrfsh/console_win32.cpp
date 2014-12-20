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

void Console_Win32::UpdatePartialCommand(int currentLineLength)
{
    AdvanceCursorPos(-1 * currentLineLength);
    EchoString(wstring(currentLineLength, L' '));
    AdvanceCursorPos(-1 * currentLineLength);

    EchoString(m_inputLines[m_currentInputLineIdx]);
}

string Console_Win32::GetInput()
{
    wstring line;
    size_t linePos = 0;

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
                if (rec.bKeyDown && linePos > 0)
                {
                    AdvanceCursorPos(-1);
                    EchoChar(L' ');
                    AdvanceCursorPos(-1);
                    line.erase(linePos - 1);
                    linePos--;
                }
                continue;
            }

            if (rec.bKeyDown)
            {
                switch (rec.wVirtualKeyCode)
                {
                case VK_DOWN:
                    if (m_currentInputLineIdx < m_inputLines.size() - 1)
                    {
                        int len = static_cast<int>(line.size());
                        m_currentInputLineIdx++;
                        UpdatePartialCommand(len);
                        line = m_inputLines[m_currentInputLineIdx];
                        linePos = line.size();
                    }
                    break;
                case VK_UP:
                    if (m_currentInputLineIdx > 0)
                    {
                        int len = static_cast<int>(line.size());
                        m_currentInputLineIdx--;
                        UpdatePartialCommand(len);
                        line = m_inputLines[m_currentInputLineIdx];
                        linePos = line.size();
                    }
                    break;
                case VK_LEFT:
                    if (line.size() > 0 && linePos > 0)
                    {
                        AdvanceCursorPos(-1);
                        linePos--;
                        // TODO: update line pos
                        // TODO: add line moving logic
                    }
                    break;
                case VK_RIGHT:
                    if (line.size() > linePos)
                    {
                        AdvanceCursorPos(1);
                        linePos++;
                    }
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
                    if (rec.wVirtualKeyCode != VK_RETURN)
                    {
                        line.insert(linePos, 1, rec.uChar.UnicodeChar);
                        linePos++;
                        if (linePos < line.size())
                        {
                            // Echo the rest of the line because we inserted
                            // in the middle of the line.
                            EchoString(line.substr(linePos));
                            AdvanceCursorPos(static_cast<int>(linePos) - static_cast<int>(line.size()));
                        }
                    }
                    m_lastKeyInputDown = true;
                }

                if (!rec.bKeyDown)
                {
                    m_lastKeyInputDown = false;

                    if (rec.wVirtualKeyCode == VK_RETURN)
                    {
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