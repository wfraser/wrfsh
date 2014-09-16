#include "unicodehack.h"

#include <iostream>
#include <string>
#include <vector>

#include "process.h"

using namespace std;

Process::Process(const string program, const vector<string> args) :
    m_program(program),
    m_args(args)
{
#ifdef _MSC_VER
    // Commands implemented by cmd.exe
    if (m_program == "dir")
    {
        vector<string> new_args({ "/c", m_program });
        new_args.insert(new_args.end(), m_args.begin(), m_args.end());
        swap(m_args, new_args);
        m_program = "cmd";
    }
#endif
}

bool Process::Run(istream& in, ostream& out, ostream& err, int *pExitCode)
{
#ifdef _MSC_VER
    return Run_Win32(in, out, err, pExitCode);
#else
    return Run_Posix(in, out, err, pExitCode);
#endif
}