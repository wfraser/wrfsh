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

    struct Input
    {
        enum class Type
        {
            Character = 0,
            Special = 1,
        };

        // These values are the same as the VK_* codes in Win32.
        enum class Special
        {
            Eof = 0x0,
            Backspace = 0x08,
            Tab = 0x09,
            Return = 0x0d,
            Left = 0x25,
            Up = 0x26,
            Right = 0x27,
            Down = 0x28,
            Delete = 0x2e,
        };

        Type type;
        union
        {
            native_string_t::value_type character;
            Special special;
        };
    };

    static Console* make();
    virtual ~Console();

    std::string get_input_line();
    void prompt();

    virtual Input get_input_char() = 0;
    virtual void write_output(const std::string& s, CharAttr attrs = CharAttr::None) = 0;
    virtual std::ostream& ostream() = 0;
    virtual void advance_cursor_pos(int n) = 0;

protected:
    void new_empty_line();
    void replace_current_line(int newIndex);
    void clear_current_display_line();

    virtual void echo_char(native_string_t::value_type c, CharAttr attrs = CharAttr::None) = 0;
    virtual void echo_string(const native_string_t& s, CharAttr attrs = CharAttr::None) = 0;

    std::vector<native_string_t> m_inputLines;
    size_t m_currentInputLineIdx;
    size_t m_currentInputLinePos;
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

    virtual Console::Input get_input_char();
    virtual void write_output(const std::string& s, CharAttr attrs = CharAttr::None);
    virtual std::ostream& ostream();
    virtual void advance_cursor_pos(int n);

protected:
    virtual void echo_char(wchar_t c, CharAttr attrs = CharAttr::None)
    {
        echo_char(c, static_cast<WORD>(attrs));
    }
    virtual void echo_string(const std::wstring& s, CharAttr attrs = CharAttr::None)
    {
        echo_string(s, static_cast<WORD>(attrs));
    }

private:
    void echo_char(wchar_t c, WORD attrs = 0);
    void echo_string(const std::wstring& s, WORD attrs = 0);
    void get_window_info();

    static BOOL WINAPI ctrl_handler(DWORD dwCtrlType);

    HANDLE m_inputHandle;
    HANDLE m_outputHandle;
    COORD m_cursorPos;
    COORD m_windowSize;

    std::unique_ptr<Console_streambuf> m_streambuf;
    std::ostream m_ostream;

    bool m_lastKeyInputDown; // hack

    static Console_Win32* s_pInstance;
};

#else

class Console_Posix : public Console
{
public:
    Console_Posix();
    ~Console_Posix();

    virtual Console::Input get_input_char();
    virtual void write_output(const std::string& s, CharAttr attrs = CharAttr::None);
    virtual std::ostream& ostream();
    virtual void advance_cursor_pos(int n);

protected:
    virtual void echo_char(char c, CharAttr attrs = CharAttr::None);
    virtual void echo_string(const std::string& s, CharAttr attrs = CharAttr::None);

private:
    bool vt_escape(Console::Input* input);

    std::unique_ptr<Console_streambuf> m_streambuf;
    std::ostream m_ostream;

    struct Details;
    Details* m_details;
};

#endif
