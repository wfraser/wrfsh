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

BOOL WINAPI Console_Win32::CtrlHandler(DWORD dwCtrlType)
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
                instance->ClearCurrentDisplayLine();
                instance->m_inputLines[instance->m_currentInputLineIdx].clear();
            }
            else
            {
                // Current line is one previously entered.
                // Switch to the empty line.
                instance->NewEmptyLine();
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

    GetWindowInfo();

    m_currentInputLineIdx = 0;

    assert(s_pInstance == nullptr);
    s_pInstance = this;

    // FIXME: this doesn't work for some reason.
    CHKERR(SetConsoleCtrlHandler(&CtrlHandler, true));
}

Console_Win32::~Console_Win32()
{
    // TODO: set console mode back to whatever the default is.
    CHKERR(SetConsoleCtrlHandler(nullptr, false));
    s_pInstance = nullptr;
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

void Console_Win32::ReplaceCurrentLine(int newIndex)
{
    ClearCurrentDisplayLine();

    m_currentInputLineIdx = newIndex;
    EchoString(m_inputLines[m_currentInputLineIdx]);
    m_currentInputLinePos = static_cast<int>(m_inputLines[m_currentInputLineIdx].size());
}

void Console_Win32::NewEmptyLine()
{
    if (m_inputLines.empty() || !m_inputLines.back().empty())
    {
        m_inputLines.push_back(L"");
    }
    ReplaceCurrentLine(static_cast<int>(m_inputLines.size()) - 1);
}

void Console_Win32::ClearCurrentDisplayLine()
{
    int currentLineLen = static_cast<int>(m_inputLines[m_currentInputLineIdx].size());

    AdvanceCursorPos(-1 * m_currentInputLinePos);
    EchoString(wstring(currentLineLen, L' '));
    AdvanceCursorPos(-1 * currentLineLen);
    m_currentInputLinePos = 0;
}

string Console_Win32::GetInput()
{
    NewEmptyLine();

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
                if (rec.bKeyDown && m_currentInputLinePos > 0)
                {
                    m_currentInputLinePos--;
                    m_inputLines[m_currentInputLineIdx].erase(m_currentInputLinePos, 1);
                    AdvanceCursorPos(-1);
                    if (m_currentInputLinePos == m_inputLines[m_currentInputLineIdx].size())
                    {
                        EchoChar(L' ');
                        AdvanceCursorPos(-1);
                    }
                    else
                    {
                        auto str = m_inputLines[m_currentInputLineIdx].substr(m_currentInputLinePos);
                        EchoString(str);
                        EchoChar(L' ');
                        AdvanceCursorPos(-1 - static_cast<int>(str.size()));
                    }
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
                        ReplaceCurrentLine(m_currentInputLineIdx + 1);
                        m_currentInputLinePos = static_cast<int>(m_inputLines[m_currentInputLineIdx].size());
                    }
                    break;
                case VK_UP:
                    if (m_currentInputLineIdx > 0)
                    {
                        ReplaceCurrentLine(m_currentInputLineIdx - 1);
                        m_currentInputLinePos = static_cast<int>(m_inputLines[m_currentInputLineIdx].size());
                    }
                    break;
                case VK_LEFT:
                    if (!m_inputLines[m_currentInputLineIdx].empty()
                        && m_currentInputLinePos > 0)
                    {
                        AdvanceCursorPos(-1);
                        m_currentInputLinePos--;
                    }
                    break;
                case VK_RIGHT:
                    if (m_inputLines[m_currentInputLineIdx].size() > m_currentInputLinePos)
                    {
                        AdvanceCursorPos(1);
                        m_currentInputLinePos++;
                    }
                    break;
                }
            }

            if ((rec.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0)
            {
                // Don't process ctrl-key events.
                continue;
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
                        m_inputLines[m_currentInputLineIdx].insert(m_currentInputLinePos, 1, rec.uChar.UnicodeChar);
                        m_currentInputLinePos++;
                        if (m_currentInputLinePos < m_inputLines[m_currentInputLineIdx].size())
                        {
                            // Echo the rest of the line because we inserted
                            // in the middle of the line.
                            EchoString(m_inputLines[m_currentInputLineIdx].substr(m_currentInputLinePos));
                            AdvanceCursorPos(static_cast<int>(m_currentInputLinePos) - static_cast<int>(m_inputLines[m_currentInputLineIdx].size()));
                        }
                    }
                    m_lastKeyInputDown = true;
                }

                if (!rec.bKeyDown)
                {
                    m_lastKeyInputDown = false;

                    if (rec.wVirtualKeyCode == VK_RETURN)
                    {
                        m_currentInputLinePos = 0;
                        return Narrow(m_inputLines[m_currentInputLineIdx]);
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