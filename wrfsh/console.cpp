#include "unicodehack.h"

#include <string>
#include <memory>
#include <vector>
#include <deque>
#include <streambuf>
#include <iostream>
#include <unordered_map>
#include <list>
#include <regex>

#include "common.h"
#include "global_state.h"
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

Console::Console()
    : m_currentInputLineIdx(0)
    , m_currentInputLinePos(0)
{
}

Console::~Console()
{
}

void Console::prompt(global_state& state)
{
    write_output(state.lookup_var("USER"), CharAttr::FG_Green);
    write_output("@");
    write_output(state.lookup_var("HOST"), CharAttr::FG_Green);
    write_output(" ");

    string cwd = state.lookup_var("PWD");
    string home = state.lookup_var("HOME");

    if (!home.empty() && home.size() <= cwd.size())
    {
#ifdef _MSC_VER
        if (0 == compare_string_nocase(cwd, home, static_cast<int>(home.size())))
#else
        if (cwd.find(home) == 0)
#endif
        {
            cwd.replace(cwd.begin(), cwd.begin() + home.size(), "~");
        }
    }

#ifdef _MSC_VER
    for (size_t i = 0, n = cwd.size(); i < n; i++)
    {
        if (cwd[i] == '\\')
            cwd[i] = '/';
    }
#endif

    write_output(cwd, CharAttr::FG_Green);
    write_output("> ", CharAttr::FG_Green | CharAttr::FG_Bold);
}

string Console::get_input_line()
{
    new_empty_line();

    for (;;)
    {
        Input c = get_input_char();
        if (c.type == Input::Type::Special)
        {
            switch (c.special)
            {
            case Input::Special::Eof:
                return "echo \"Console EOF\"\nexit";
            case Input::Special::Down:
                if (m_currentInputLineIdx < m_inputLines.size() - 1)
                {
                    replace_current_line(static_cast<int>(m_currentInputLineIdx) + 1);
                    m_currentInputLinePos = m_inputLines[m_currentInputLineIdx].size();
                }
                break;
            case Input::Special::Up:
                if (m_currentInputLineIdx > 0)
                {
                    replace_current_line(static_cast<int>(m_currentInputLineIdx) - 1);
                    m_currentInputLinePos = m_inputLines[m_currentInputLineIdx].size();
                }
                break;
            case Input::Special::Left:
                if (!m_inputLines[m_currentInputLineIdx].empty()
                    && m_currentInputLinePos > 0)
                {
                    advance_cursor_pos(-1);
                    --m_currentInputLinePos;
                }
                break;
            case Input::Special::Right:
                if (m_inputLines[m_currentInputLineIdx].size() > m_currentInputLinePos)
                {
                    advance_cursor_pos(1);
                    ++m_currentInputLinePos;
                }
                break;
            case Input::Special::Home:
                advance_cursor_pos(-1 * static_cast<int>(m_currentInputLinePos));
                m_currentInputLinePos = 0;
                break;
            case Input::Special::End:
                {
                    size_t size = m_inputLines[m_currentInputLineIdx].size();
                    advance_cursor_pos(static_cast<int>(size - m_currentInputLinePos));
                    m_currentInputLinePos = size;
                }
                break;
            case Input::Special::Return:
                m_currentInputLinePos = 0;
                echo_char('\n');
                return Narrow(m_inputLines[m_currentInputLineIdx]);
            case Input::Special::Backspace:
                if (m_currentInputLinePos > 0)
                {
                    --m_currentInputLinePos;
                    m_inputLines[m_currentInputLineIdx].erase(m_currentInputLinePos, 1);
                    advance_cursor_pos(-1);
                    native_string_t str = m_inputLines[m_currentInputLineIdx].substr(m_currentInputLinePos);
                    echo_string(str);
                    echo_char(L' ');
                    advance_cursor_pos(-1 - static_cast<int>(str.size()));
                }
                break;
            case Input::Special::Delete:
                if (m_currentInputLinePos < m_inputLines[m_currentInputLineIdx].size())
                {
                    m_inputLines[m_currentInputLineIdx].erase(m_currentInputLinePos, 1);
                    native_string_t str = m_inputLines[m_currentInputLineIdx].substr(m_currentInputLinePos);
                    echo_string(str);
                    echo_char(L' ');
                    advance_cursor_pos(-1 - static_cast<int>(str.size()));
                }
                else
                    ding();
                break;
            default:
                throw new exception();
            }
        }
        else // (c.type == Input::Type::Character)
        {
            echo_char(c.character);
            m_inputLines[m_currentInputLineIdx].insert(m_currentInputLinePos, 1, c.character);
            ++m_currentInputLinePos;
            if (m_currentInputLinePos < m_inputLines[m_currentInputLineIdx].size())
            {
                // Echo the rest of the line because we inserted in the middle of the line.
                echo_string(m_inputLines[m_currentInputLineIdx].substr(m_currentInputLinePos));
                advance_cursor_pos(static_cast<int>(m_currentInputLinePos) - static_cast<int>(m_inputLines[m_currentInputLineIdx].size()));
            }
        }
    } // for(;;)
}

void Console::new_empty_line()
{
    if (m_inputLines.empty() || !m_inputLines.back().empty())
    {
        m_inputLines.push_back(native_string_t());
    }
    replace_current_line(static_cast<int>(m_inputLines.size()) - 1);
}

void Console::replace_current_line(int newIndex)
{
    clear_current_display_line();

    m_currentInputLineIdx = newIndex;
    echo_string(m_inputLines[m_currentInputLineIdx]);
    m_currentInputLinePos = static_cast<int>(m_inputLines[m_currentInputLineIdx].size());
}

void Console::clear_current_display_line()
{
    int currentLineLen = 0;
    if (!m_inputLines.empty())
        currentLineLen = static_cast<int>(m_inputLines[m_currentInputLineIdx].size());

    advance_cursor_pos(-1 * static_cast<int>(m_currentInputLinePos));
    echo_string(native_string_t(currentLineLen, static_cast<native_string_t::value_type>(' ')));
    advance_cursor_pos(-1 * currentLineLen);
    m_currentInputLinePos = 0;
}
