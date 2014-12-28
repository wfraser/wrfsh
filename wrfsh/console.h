#pragma once

class Console
{
public:
    enum class CharAttr
    {
        None = 0,

        FG_Blue = 0x1,
        FG_Green = 0x2,
        FG_Red = 0x4,
        FG_Bold = 0x8,

        BG_Blue = 0x10,
        BG_Green = 0x20,
        BG_Red = 0x40,
        BG_Bold = 0x80,

        Reverse = 0x4000,
        Underline = 0x8000,
    };

    static Console* make();

    virtual std::string get_input() = 0;
    virtual void write_output(const std::string& s, CharAttr attrs = CharAttr::None) = 0;
    virtual std::ostream& ostream() = 0;
    virtual void advance_cursor_pos(int n) = 0;

    void prompt();
};

inline Console::CharAttr operator|(Console::CharAttr x, Console::CharAttr y)
{
    return static_cast<Console::CharAttr>(
        static_cast<unsigned int>(x) | static_cast<unsigned int>(y)
        );
}

class Console_streambuf : public std::streambuf
{
public:
    Console_streambuf(Console* con) : m_console(con)
    {}

    Console_streambuf(const Console_streambuf& other)
        : m_console(other.m_console)
    {}

    int_type overflow(int_type c = traits_type::eof())
    {
        if (c == traits_type::eof())
        {
            return traits_type::eof();
        }
        else
        {
            char_type ch = traits_type::to_char_type(c);
            return xsputn(&ch, 1) == 1 ? c : traits_type::eof();
        }
    }

    std::streamsize xsputn(const char* s, std::streamsize n)
    {
        m_console->write_output(std::string(s, n));
        return n;
    }

private:
    Console* m_console;
};

#ifdef _MSC_VER

class Console_Win32 : public Console
{
public:
    Console_Win32();
    ~Console_Win32();

    virtual std::string get_input();
    virtual void write_output(const std::string& s, CharAttr attrs = CharAttr::None);
    virtual std::ostream& ostream();
    virtual void advance_cursor_pos(int n);

private:
    void get_window_info();
    void echo_char(wchar_t c, WORD attrs = 0);
    void echo_string(const std::wstring& s, WORD attrs = 0);
    void new_empty_line();
    void replace_current_line(int newIndex);
    void clear_current_display_line();

    static BOOL WINAPI ctrl_handler(DWORD dwCtrlType);

    HANDLE m_inputHandle;
    HANDLE m_outputHandle;
    std::vector<std::wstring> m_inputLines;
    int m_currentInputLineIdx;
    int m_currentInputLinePos;

    COORD m_cursorPos;
    COORD m_windowSize;

    std::unique_ptr<Console_streambuf> m_streambuf;
    std::ostream m_ostream;

    bool m_lastKeyInputDown; // hack

    static Console_Win32* s_pInstance;
};

#else

class Console_Posix : public Console
{};

#endif
