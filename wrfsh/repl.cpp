#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <locale>

#include "common.h"
#include "global_state.h"

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
    (void)in;
    (void)out;
    (void)err;
    (void)args;

    //TODO
    bool if_result = false;

    state.if_state.push_back({ if_result });

    return 0;
}

int else_commandlet(istream& in, ostream& out, ostream& err, global_state& state, vector<string>& args)
{
    if (state.if_state.size() == 0)
    {
        err << "Syntax error: else without preceding if\n";
        state.error = true;
        return -1;
    }

    if (args.size() > 0)
    {
        if (args[0] == "if")
        {
            if (!state.if_state.back().active)
            {
                if_commandlet(in, out, err, state, args);
                bool condition = state.if_state.back().active;
                state.if_state.pop_back();
                state.if_state.back().active = condition;
            }
        }
        else
        {
            err << "Syntax error: else does not take any arguments (except when of the form 'else if')\n";
            state.error = true;
            return -1;
        }
    }
    else
    {
        state.if_state.back().active ^= true;
    }

    return 0;
}

int endif_commandlet(istream& in, ostream& out, ostream& err, global_state& state, vector<string>& args)
{
    (void)in;
    (void)out;
    (void)args;

    if (args.size() != 0)
    {
        err << "Syntax error: endif does not take any arguments";
        state.error = true;
        return -1;
    }

    if (state.if_state.size() == 0)
    {
        err << "Syntax error: endif without preceding if\n";
        state.error = true;
        return -1;
    }

    state.if_state.pop_back();

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

int list_commandlet(istream& /*in*/, ostream& out, ostream& /*err*/, global_state& state, vector<string>& /*args*/)
{
    for (const auto& line : state.stored_program)
    {
        out << line.number << " " << line.command;
        for (size_t i = 0, n = line.args.size(); i < n; i++)
        {
            out << " " << line.args.at(i) << ((i == n - 1) ? "\n" : "");
        }
    }
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
    DEFINE_COMMANDLET(list),
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
        if ((!global_state.if_state.size() == 0)
            && !global_state.if_state.back().active
            && (command != "else")
            && (command != "endif"))
        {
            return 0;
        }

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
    string::size_type variable_dollar_pos = 0;

    bool in_comment = false;

    enum class readstate
    {
        reading_command,
        reading_args
    };
    readstate state = readstate::reading_command;

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
                string& s = (state == readstate::reading_command) ? command.command : command.args.back();

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
                if (!command.command.empty() && (command.args.size() > 0) && command.args.back().empty())
                {
                    // Remove the empty last argument if present.
                    command.args.pop_back();
                }

                if (!command.special.empty())
                {
                    int number = atoi(command.special.c_str());
                    bool found = false;

                    auto it = global_state.stored_program.begin();

                    for (auto end = global_state.stored_program.end(); it != end; ++it)
                    {
                        int current = atoi(it->number.c_str());

                        if (command.command.empty())
                        {
                            if (current == number)
                            {
                                global_state.stored_program.erase(it);
                                break;
                            }
                        }
                        else if (current == number)
                        {
                            it->command = move(command.command);
                            it->args = move(command.args);
                            found = true;
                        }
                        else if (current > number)
                        {
                            // Keep the current iterator position.
                            break;
                        }
                    }

                    if (!found && !command.command.empty())
                    {
                        global_state.stored_program.emplace(it, move(command.special), move(command.command), move(command.args));
                    }

                    command.reset();
                    state = readstate::reading_command;
                }
                else if (!command.command.empty())
                {
                    command.print(out); //DEBUG

                    int retval = command.execute(in, out, err, global_state);

                    // Save the return value as $?
                    char buf[10];
                    snprintf(buf, 10, "%d", retval);
                    global_state.let("?", buf);

                    command.reset();
                    state = readstate::reading_command;
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
                    if (state == readstate::reading_command && !command.command.empty())
                    {
                        state = readstate::reading_args;
                    }
                    else if (state == readstate::reading_args && !command.args.back().empty())
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
                if (!in_string_singlequote && !variable_pending && command.special.empty())
                {
                    variable_pending = true;
                    variable_dollar_pos = (state == readstate::reading_command) ? command.command.size() : command.args.back().size();
                }
                goto normal;

            normal:
            default:
                if (state == readstate::reading_command)
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
