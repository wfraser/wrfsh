#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <list>
#include <locale>
#include <memory>
#include <algorithm>
#include <functional>

#include "common.h"
#include "global_state.h"
#include "commandlets.h"

using namespace std;

string process_expression(const string& expression, global_state& global_state)
{
    string result;

    bool variable_pending = false;
    size_t var_substitution_start_pos = 0;
    bool backtick_pending = false;
    size_t bt_substitution_start_pos = 0;
    bool in_string = false;
    bool in_string_singlequote = false;
    bool escape = false;
    for (size_t i = 0, n = expression.size(); i <= n; i++)
    {
        const char c = expression[i];

        if (variable_pending)
        {
            string::size_type len = result.size() - var_substitution_start_pos;

            static const string allowed_variable_special_characters = "#*@!_?$";
            // $# = number of positional parameters
            // $* = positional parameters strung together as a single word
            // $@ = positional parameters *as separate words* (doesn't work yet)
            // $! = PID of last job run in background (doesn't work yet)
            // $_ = last positional parameter of previous command (doesn't work yet)
            // $? = exit status of previous command
            // $$ = current PID (doesn't work yet)

            if (i == n
                || (
                    len > 1
                    && !isalnum(c, locale::classic())
                    && allowed_variable_special_characters.find(c) == string::npos))
            {
                // A variable was ended.
                string varname = result.substr(var_substitution_start_pos + 1, len - 1);
                string value = global_state.lookup_var(varname);
                result.replace(var_substitution_start_pos, len, value);
                variable_pending = false;
            }

            if (i == n)
            {
                break;
            }
        }

        if (escape)
        {
            goto normal;
        }

        switch (c)
        {
        case '"':
            if (!in_string)
            {
                in_string = true;
            }
            else if (!in_string_singlequote)
            {
                in_string = false;
            }
            else
            {
                goto normal;
            }
            break;

        case '\'':
            if (!in_string)
            {
                in_string = true;
                in_string_singlequote = true;
            }
            else if (in_string_singlequote)
            {
                in_string = false;
                in_string_singlequote = false;
            }
            else

            {
                goto normal;
            }
            break;

        case '`':
            if (!in_string_singlequote)
            {
                if (backtick_pending)
                {
                    backtick_pending = false;
                    string command_line = result.substr(bt_substitution_start_pos);
                    // TODO: run command, substitute output
                    string output = "command output";
                    result.replace(bt_substitution_start_pos, result.size(), output);
                }
                else
                {
                    backtick_pending = true;
                    bt_substitution_start_pos = result.size();
                }
            }
            else
            {
                goto normal;
            }

        case '$':
            if (!in_string_singlequote && !variable_pending)
            {
                variable_pending = true;
                var_substitution_start_pos = result.size();
            }
            goto normal;

        case '\\':
            escape = true;
            break;

        normal:
        default:
            result.push_back(c);
            escape = false;
            break;
        }
    }

    return result;
}

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
        if ((global_state.if_state.size() != 0)
            && !global_state.if_state.back().active
            && (command != "else")
            && (command != "endif"))
        {
            return 0;
        }

        vector<string> new_args;
        bool args_processed = false;
        if (command != "if" && command != "else") // these use the un-processed strings
        {
            // Do string interpolation, backtick expansion, etc.
            for (const auto& arg : args)
            {
                new_args.push_back(process_expression(arg, global_state));
            }
            args_processed = true;
            swap(args, new_args);
        }

        int retval;

        auto pos = special_functions.find(command);
        if (pos != special_functions.end())
        {
            retval = pos->second(in, out, err, global_state, args);
        }
        else
        {
            // Not a commandlet.
            //TODO run the command
            retval = 99;
        }

        if (args_processed)
        {
            swap(args, new_args);
        }

        return retval;
    }
};

int repl(istream& in, ostream& out, ostream& err, global_state& global_state)
{
    int exitCode = 0;
    program_line command;

    bool escape = false;

    bool in_string = false;
    bool in_string_singlequote = false;

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
                }
                else
                {
                    in_string = true;
                    in_string_singlequote = true;
                }
                goto normal;

            case '"':
                if (in_string)
                {
                    if (!in_string_singlequote)
                    {
                        in_string = false;
                    }
                }
                else
                {
                    in_string = true;
                }
                goto normal;

            case '`':
                //TODO
                goto normal;

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
                if (!command.special.empty())
                {
                    goto normal;
                }
                break;

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
