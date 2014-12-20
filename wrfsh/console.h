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

    static Console* Make();

    virtual std::string GetInput() = 0;
    virtual void WriteOutput(const std::string& s, CharAttr attrs = CharAttr::None) = 0;
    virtual std::ostream& ostream() = 0;
    virtual void AdvanceCursorPos(int n) = 0;

    void Prompt();
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
        m_console->WriteOutput(std::string(s, n));
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

    virtual std::string GetInput();
    virtual void WriteOutput(const std::string& s, CharAttr attrs = CharAttr::None);
    virtual std::ostream& ostream();
    virtual void AdvanceCursorPos(int n);

private:
    void GetWindowInfo();
    void EchoChar(wchar_t c, WORD attrs = 0);
    void EchoString(const std::wstring& s, WORD attrs = 0);
    void UpdatePartialCommand(int currentLineLength);

    HANDLE m_inputHandle;
    HANDLE m_outputHandle;
    std::vector<std::wstring> m_inputLines;
    size_t m_currentInputLineIdx;

    COORD m_cursorPos;
    COORD m_windowSize;

    std::unique_ptr<Console_streambuf> m_streambuf;
    std::ostream m_ostream;

    bool m_lastKeyInputDown; // hack
};

#else

class Console_Posix : public Console
{};

#endif