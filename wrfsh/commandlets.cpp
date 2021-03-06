#include "unicodehack.h"

#include <iostream>
#include <vector>
#include <list>
#include <unordered_map>
#include <string>
#include <memory>
#include <algorithm>
#include <functional>
#include <sstream>
#include <string.h>

#include "common.h"
#include "global_state.h"
#include "repl.h"
#include "commandlets.h"

using namespace std;

int let_commandlet(istream& /*in*/, ostream& out, ostream& err, global_state& state, vector<string>& args)
{
    if (args.size() == 0)
    {
        for (const auto& pair : state.environment)
        {
            if (state.local_vars.find(pair.first) == state.local_vars.end())
            {
                out << pair.first << "=" << pair.second << endl;
            }
        }
        for (const auto& pair : state.local_vars)
        {
            out << pair.first << "=" << pair.second << endl;
        }
    }
    else if (args.size() != 3 || args[1] != "=")
    {
        err << "Syntax error: 'let' expects to be used like: 'let {variable} = {value}'\n";
        state.error = true;
        return -1;
    }
    else
    {
        state.let(args[0], args[2]);
    }

    return 0;
}

int echo_commandlet(istream& /*in*/, ostream& out, ostream& /*err*/, global_state& /*state*/, vector<string>& args)
{
    for (size_t i = 0, n = args.size(); i < n; i++)
    {
        out << args[i];
        if (i != n - 1)
            out << " ";
    }
    out << endl;
    return 0;
}

int list_commandlet(istream& /*in*/, ostream& out, ostream& err, global_state& state, vector<string>& args)
{
    if (args.size() > 1)
    {
        err << "Syntax error: \"list\" only takes one argument, a line number to print.\n";
        state.error = true;
        return -1;
    }

    for (const auto& line : state.stored_program)
    {
        bool match = false;
        if (args.size() == 1)
        {
            if (line.number == args[0])
            {
                match = true;
            }
            else
            {
                continue;
            }
        }

        out << line.number << " " << line.command;
        for (const auto& arg : line.args)
        {
            out << " " << arg;
        }
        out << endl;

        if (match)
        {
            break;
        }
    }
    return 0;
}

int run_commandlet(istream& in, ostream& out, ostream& err, global_state& state, vector<string>& args)
{
    if (!state.interactive)
    {
        err << "Error: the stored program functionality only works in an interactive session.\n";
        state.error = true;
        return -1;
    }

    if (args.size() > 1)
    {
        err << "Syntax error: \"run\" only takes one argument, a line number to start at.\n";
        state.error = true;
        return -1;
    }

    int start = 0;
    if (args.size() == 1)
    {
        start = atoi(args[0].c_str());
    }
    
    state.interactive = false;
    stringstream program_stream;
    for (const auto& line : state.stored_program)
    {
        if (atoi(line.number.c_str()) >= start)
        {
            program_stream << line.command;
            for (const auto& arg : line.args)
            {
                program_stream << " " << arg;
            }
            program_stream << endl;
        }
    }
    int retval = repl(program_stream, out, err, state, in);
    state.interactive = true;

    return retval;
}

int new_commandlet(istream& /*in*/, ostream& /*out*/, ostream& err, global_state& state, vector<string>& args)
{
    if (args.size() != 0)
    {
        err << "Syntax error: \"new\" doesn't take any arguments.\n";
        state.error = true;
        return -1;
    }

    state.stored_program.clear();

    return 0;
}

int exit_commandlet(istream& /*in*/, ostream& /*out*/, ostream& err, global_state& state, vector<string>& args)
{
    int exitCode;

    switch (args.size())
    {
    case 0:
        exitCode = 0;
        state.exit = true;
        break;

    case 1:
    {
        size_t badchar_pos;
        try
        {
            exitCode = stoi(args[0], &badchar_pos, 10);
        }
        catch (...)
        {
            badchar_pos = 0;
            exitCode = -1;
        }
        if (args[0][badchar_pos] != '\0')
        {
            err << "Syntax error: 'exit' expects a numeric argument.\n";
            state.error = true;
            exitCode = -1;
        }
        else
        {
            state.exit = true;
        }
        break;
    }

    default:
        err << "Syntax error: too many arguments to 'exit'. Only one optional argument is expected.\n";
        state.error = true;
        exitCode = -1;
        break;
    }

    return exitCode;
}

int cd_commandlet(istream& /*in*/, ostream& /*out*/, ostream& err, global_state& state, vector<string>& args)
{
    string new_cwd;
    if (args.size() == 0)
        new_cwd = state.lookup_var("HOME");
    else if (args.size() == 1)
        new_cwd = args[0];
    else
    {
        err << "Syntax error: too many arguments to 'cd'. Only one optional argument is expected.\n";
        state.error = true;
        return -1;
    }

#ifdef _MSC_VER
    if (SetCurrentDirectoryW(Widen(new_cwd).c_str()) == 0)
    {
        DWORD error = GetLastError();
        err << "cd: " << Narrow(_com_error(HRESULT_FROM_WIN32(error)).ErrorMessage()) << endl;
        state.error = true;
        return error;
    }
#else
    if (chdir(new_cwd.c_str()) != 0)
    {
        err << "cd: " << strerror(errno) << endl;
        state.error = true;
        return errno;
    }
#endif

    if (!state.error)
    {
        state.let("OLDPWD", state.lookup_var("PWD"));
        state.let("PWD", get_current_working_directory(err));
    }

    return 0;
}

int pwd_commandlet(istream& /*in*/, ostream& out, ostream& err, global_state& state, vector<string>& args)
{
    if (args.size() != 0)
    {
        err << "Syntax error: 'pwd' expects no arguments.\n";
        state.error = true;
        return -1;
    }

    out << get_current_working_directory(err) << endl;

    return 0;
}

#define DEFINE_COMMANDLET(name) { #name, name##_commandlet }

unordered_map<string, commandlet_function> special_functions(
{
    DEFINE_COMMANDLET(let),
    DEFINE_COMMANDLET(if),      // defined in if_else_endif.cpp
    DEFINE_COMMANDLET(else),    //
    DEFINE_COMMANDLET(endif),   //
    DEFINE_COMMANDLET(echo),
    DEFINE_COMMANDLET(list),
    DEFINE_COMMANDLET(run),
    DEFINE_COMMANDLET(new),
    DEFINE_COMMANDLET(exit),
    DEFINE_COMMANDLET(cd),
    DEFINE_COMMANDLET(pwd),
});
