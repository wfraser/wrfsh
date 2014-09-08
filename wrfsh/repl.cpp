#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <locale>

#include "global_state.h"

#ifdef _MSC_VER
#define snprintf(buf, n, format, ...)  _snprintf_s(buf, n, n, format, __VA_ARGS__)
#endif

using namespace std;

typedef int(*commandlet_function)(istream& in, ostream& out, ostream& err, global_state& gs, vector<string>& args);

int let_commandlet(istream& /*in*/, ostream& /*out*/, ostream& err, global_state& state, vector<string>& args)
{
    if (args.size() != 3 || args[1] != "=")
    {
        err << "Syntax error: 'let' expects to be used like: 'let {variable} = {value}'\n";
        state.error = true;
        return -1;
    }

    state.let(args[0], args[2]);
    return 0;
}

int if_commandlet(istream& in, ostream& out, ostream& err, global_state& state, vector<string>& args)
{
    (void)(in, out, err, state, args);
    //TODO
    return 0;
}

int else_commandlet(istream& in, ostream& out, ostream& err, global_state& state, vector<string>& args)
{
    (void)(in, out, err, state, args);
    //TODO
    return 0;
}

int endif_commandlet(istream& in, ostream& out, ostream& err, global_state& state, vector<string>& args)
{
    (void)(in, out, err, state, args);
    //TODO
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

#define DEFINE_COMMANDLET(name) { #name, name##_commandlet }

unordered_map<string, commandlet_function> special_functions(
{
    DEFINE_COMMANDLET(let),
    DEFINE_COMMANDLET(if),
    DEFINE_COMMANDLET(else),
    DEFINE_COMMANDLET(endif),
    DEFINE_COMMANDLET(echo),
});

struct program_line
{
    string special;
    string command;
    vector<string> args;

    program_line() :
        special(),
        command(),
        args({ string() }) // ensure there's an empty arg to push characters onto
    {
    }

    void reset()
    {
        special.clear();
        command.clear();
        args.clear();
        args.emplace_back();
    }

    void print(ostream& out)
    {
        if (!special.empty())
            out << "(" << special << ") ";
        out << command << endl;
        for (size_t i = 0, n = args.size(); i < n; i++)
        {
            if (!args[i].empty())
            {
                out << "\t" << i << ": " << args[i] << endl;
            }
        }
    }

    int execute(istream& in, ostream& out, ostream& err, global_state& global_state)
    {
        auto pos = special_functions.find(command);
        if (pos != special_functions.end())
        {
            return pos->second(in, out, err, global_state, args);
        }
        else
        {
            // Not a commandlet.
            //TODO run the command
            return 99;
        }
    }
};

int repl(istream& in, ostream& out, ostream& err, global_state& global_state)
{
    int exitCode = 0;
    program_line command;
    bool escape = false;
    bool in_string = false;
    bool in_string_singlequote = false; // disables variable interpolation
    bool variable_pending = false;
    bool in_comment = false;
    string::size_type variable_dollar_pos = 0;
    enum class state
    {
        reading_command,
        reading_args
    };
    state state = state::reading_command;

    for (;;)
    {
        char c;
        in.get(c);

        if (in.bad() && !in.eof())
        {
            err << "badbit on input\n";
            exitCode = 1;
        }
        else if (in.fail() && !in.eof())
        {
            err << "failbit on input\n";
            exitCode = 2;
        }
        else
        {
            // Begin input parser state machine

            if (in.eof())
            {
                // Add a newline in case EOF came at the end of a line.
                // We'll terminate at the end of this loop iteration.
                c = '\n';
            }

            if (in_comment && c != '\n')
            {
                // Comments continue to the end of the line.
                continue;
            }

            if (variable_pending)
            {
                string& s = (state == state::reading_command) ? command.command : command.args.back();

                string::size_type len = s.size() - variable_dollar_pos;

                // if not ~ /[a-zA-Z][a-zA-Z0-9]*/
                if (len > 1 && ((len == 2) ? !isalpha(c, locale::classic()) : !isalnum(c, locale::classic())))
                {
                    // A variable was ended.
                    string varname = s.substr(variable_dollar_pos + 1, len - 1);
                    string value = global_state.lookup_var(varname);
                    s.replace(variable_dollar_pos, len, value);
                    variable_pending = false;
                }
            }

            if (escape)
            {
                // special case: newline doesn't go to the argument unless it's inside a string
                if (c == '\n' && !in_string)
                {
                    c = ' ';
                    escape = false;
                }
                else
                {
                    goto normal;
                }
            }

            switch (c)
            {
            case '\n':
                if (!command.command.empty())
                {
                    command.print(out); //DEBUG

                    int retval = command.execute(in, out, err, global_state);

                    // Save the return value as $?
                    char buf[10];
                    snprintf(buf, 10, "%d", retval);
                    global_state.let("?", buf);

                    command.reset();
                    state = state::reading_command;
                }
                else if (!command.special.empty())
                {
                    cerr << "Syntax error: " << command.special << " must be followed by a command.\n";
                    if (global_state.interactive)
                    {
                        command.reset();
                        state = state::reading_command;
                    }
                    else
                    {
                        exitCode = 3;
                        global_state.error = true;
                    }
                }

                if (in_comment)
                {
                    in_comment = false;
                }
                break;

            case ' ':
            case '\t':
                if (!in_string)
                {
                    if (state == state::reading_command && !command.command.empty())
                    {
                        state = state::reading_args;
                    }
                    else if (state == state::reading_args && !command.args.back().empty())
                    {
                        command.args.emplace_back();
                    }
                    break;
                }
                else
                {
                    goto normal;
                }

            case '#':
                in_comment = true;
                break;

            case '\'':
                if (in_string)
                {
                    if (in_string_singlequote)
                    {
                        in_string = false;
                        in_string_singlequote = false;
                    }
                    else
                    {
                        goto normal;
                    }
                }
                else
                {
                    in_string = true;
                    in_string_singlequote = true;
                }
                break;

            case '"':
                if (in_string)
                {
                    if (!in_string_singlequote)
                    {
                        in_string = false;
                    }
                    else
                    {
                        goto normal;
                    }
                }
                else
                {
                    in_string = true;
                }
                break;

            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                if (command.command.empty())
                {
                    command.special.push_back(c);
                    break;
                }
                else
                {
                    goto normal;
                }

            case '\\':
                escape = true;
                break;

            case '$':
                if (!in_string_singlequote && !variable_pending)
                {
                    variable_pending = true;
                    variable_dollar_pos = (state == state::reading_command) ? command.command.size() : command.args.back().size();
                }
                goto normal;

            normal:
            default:
                if (state == state::reading_command)
                {
                    command.command.push_back(c);
                }
                else
                {
                    command.args.back().push_back(c);
                }

                if (escape)
                {
                    escape = false;
                }
            } // end switch

            if (global_state.error)
            {
                if (!global_state.interactive)
                    break;

                global_state.error = false;
            }

            if (in.eof())
            {
                // EOF was hit this iteration, and handled above. Terminate the REPL now.
                break;
            }
        }
    } // for(;;)
    return exitCode;
}