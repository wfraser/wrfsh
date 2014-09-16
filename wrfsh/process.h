#pragma once

class Process
{
public:
    Process(const std::string program, const std::vector<std::string> args);

    bool Run(std::istream& in, std::ostream& out, std::ostream& err, int* pExitCode);

private:
    bool Run_Win32(std::istream& in, std::ostream& out, std::ostream& err, int* pExitCode);
    bool Run_Posix(std::istream& in, std::ostream& out, std::ostream& err, int* pExitCode);

    std::string m_program;
    std::vector<std::string> m_args;
};