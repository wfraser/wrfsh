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

string process_expression(const string expression, global_state& global_state)
{
    string result;

    bool variable_pending = false;
    size_t var_substitution_start_pos = 0;
    bool backtick_pending = false;
    size_t bt_substitution_start_pos = 0;
    bool in_string = false;
    bool in_string_singlequote = false;
    bool escape = false;
    for (size_t i = 0, n = expression.size(); i < n; i++)
    {
        const char c = expression[i];

        if (variable_pending)
        {
            string::size_type len = result.size() - var_substitution_start_pos;

            // if not ~ /[a-zA-Z][a-zA-Z0-9]*/
            if (len > 1 && ((len == 2) ? !isalpha(c, locale::classic()) : !isalnum(c, locale::classic())))
            {
                // A variable was ended.
                string varname = result.substr(var_substitution_start_pos + 1, len - 1);
                string value = global_state.lookup_var(varname);
                result.replace(var_substitution_start_pos, len, value);
                variable_pending = false;
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
            if (!in_string_singlequote)
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

int if_commandlet(istream& /*in*/, ostream& out, ostream& err, global_state& global_state, vector<string>& args)
{
    // EBNF:
    // initial = "if" , comparison ;
    // comparisons = comparison , { logic_operator , comparison } ;
    // comparison = "( " , expression , operator , expression , " )"
    //            | expression, operator, expression ;
    // expression = "`" , ? command_line ? , "`"
    //            | ? string ?
    //            | "$" , variable_name ;
    // operator = "==" | "!=" | "<" | "<=" | ">" | ">=" | "~" | "!~" ;
    // logic_operator = "&&" | "||" ;
    // variable_name = [a-zA-Z0-9]*

    struct Comparison
    {
        string expression1;
        string op;
        string expression2;
    };

    struct Expression;
    struct CompoundExpression
    {
        unique_ptr<Expression> expr1;
        string op;
        unique_ptr<Expression> expr2;
    };

    struct Expression
    {
        enum class Type { Empty, Comparison, CompoundExpression };
        Type type;
        unique_ptr<Comparison> comparison;
        unique_ptr<CompoundExpression> compound_expression;
    };

    function<void(ostream&, Expression&, int)> printAST =
        [&printAST](ostream& out, Expression& exp, int nesting_level) -> void
    {
        out << string(nesting_level * 4, ' ')
            << "expression type ";
        if (exp.type == Expression::Type::Empty)
        {
            out << "empty\n";
        }
        else if (exp.type == Expression::Type::Comparison)
        {
            out << "comparison:\n";
            out << string((nesting_level + 1) * 4, ' ') << exp.comparison->expression1 << endl;
            out << string((nesting_level + 1) * 4, ' ') << exp.comparison->op << endl;
            out << string((nesting_level + 1) * 4, ' ') << exp.comparison->expression2 << endl;
        }
        else if (exp.type == Expression::Type::CompoundExpression)
        {
            out << "compound:\n";
            auto e1 = exp.compound_expression->expr1.get();
            if (e1 == nullptr)
            {
                out << string((nesting_level + 1) * 4, ' ') << "null\n";
            }
            else
            {
                printAST(out, *e1, nesting_level + 1);
            }

            out << string((nesting_level + 1) * 4, ' ') << exp.compound_expression->op << endl;

            auto e2 = exp.compound_expression->expr2.get();
            if (e2 == nullptr)
            {
                out << string((nesting_level + 1) * 4, ' ') << "null\n";
            }
            else
            {
                printAST(out, *e2, nesting_level + 1);
            }
        }
        else
        {
            out << "unknown!\n";
        }
    };

    const vector<string> operators({ "==", "!=", "<", "<=", ">", ">=", "~", "!~" });
    const vector<string> logic_operators({ "&&", "||" });

    enum class State
    {
        Expression1, Comparison1, Comparison2, Expression2
    };
    State s = State::Expression1;

    Expression root_expression({});
    vector<Expression*> stack({ &root_expression });

    for (size_t i = 0, n = args.size(); i < n; i++)
    {
        string arg = args[i];

        // DEBUG: print the AST at each iteration
        //out << i << " ===========================================\n";
        //printAST(out, root_expression, 0);

        if (stack.size() == 0)
        {
            err << "Syntax error: unexpected \"" << arg << "\". The 'if' expression is already complete.\n";
            global_state.error = true;
            return -1;
        }

        switch (s)
        {
        case State::Expression1:

            if (arg == ")")
            {
                if (stack.back()->type != Expression::Type::CompoundExpression)
                {
                    err << "Syntax error: unexpected \")\" found when not in a compound expression.\n";
                    global_state.error = true;
                    //return -1;
                }
                else if (stack.back()->compound_expression->expr1 == nullptr)
                {
                    err << "Syntax error: unexpected \")\" found; expected an expression.\n";
                    global_state.error = true;
                    //return -1;
                }
                else if (stack.back()->compound_expression->expr2 == nullptr)
                {
                    err << "Syntax error: unexpected \")\" found after only one half of a compound expression.\n";
                    global_state.error = true;
                    //return -1;
                }
                else
                {
                    stack.pop_back();
                    if (stack.size() > 0
                        && stack.back()->type == Expression::Type::CompoundExpression
                        && stack.back()->compound_expression->expr1 != nullptr)
                    {
                        s = State::Expression2;
                    }
                }
            }
            else
            {
                stack.back()->type = Expression::Type::Comparison;
                stack.back()->comparison = make_unique<Comparison>();
                if (arg != "(")
                {
                    stack.back()->comparison->expression1 = arg;
                    s = State::Comparison1;
                }
            }
            break;

        case State::Comparison1:

            if (find(operators.begin(), operators.end(), arg) != operators.end())
            {
                stack.back()->comparison->op = arg;
                s = State::Comparison2;
            }
            else
            {
                err << "Syntax error: expected comparison operator, found \"" << arg << "\" instead.\n";
                global_state.error = true;
                //return -1;
            }
            break;

        case State::Comparison2:

            stack.back()->comparison->expression2 = arg;
            s = State::Expression2;
            break;

        case State::Expression2:

            if (find(logic_operators.begin(), logic_operators.end(), arg) != logic_operators.end())
            {
                if (stack.back()->type == Expression::Type::Comparison)
                {
                    stack.back()->type = Expression::Type::CompoundExpression;
                    stack.back()->compound_expression = make_unique<CompoundExpression>();
                    stack.back()->compound_expression->expr1 = make_unique<Expression>();
                    stack.back()->compound_expression->expr1->type = Expression::Type::Comparison;
                    stack.back()->compound_expression->expr1->comparison.swap(stack.back()->comparison);
                    stack.back()->compound_expression->op = arg;
                    stack.back()->compound_expression->expr2 = make_unique<Expression>();
                    stack.push_back(stack.back()->compound_expression->expr2.get());
                    s = State::Expression1;
                }
                else if (stack.back()->type == Expression::Type::CompoundExpression)
                {
                    stack.back()->compound_expression->expr2 = make_unique<Expression>();
                    stack.push_back(stack.back()->compound_expression->expr2.get());
                    s = State::Expression1;
                }
                else
                {
                    err << "Internal error. Expression type is bad in state expression2. This is a bug!\n";
                    global_state.error = true;
                    //return -1;
                }
            }
            break;
        }

        if (global_state.error)
        {
            return -1;
        }
    }

    //DEBUG print AST
    //printAST(out, root_expression, 0);
    (void) out;

    //TODO: evaluate the AST we just built
    bool if_result = false;

    global_state.if_state.push_back({ if_result });

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
                vector<string> args2(args.begin() + 1, args.end());
                if_commandlet(in, out, err, state, args2);
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

int endif_commandlet(istream& /*in*/, ostream& /*out*/, ostream& err, global_state& state, vector<string>& args)
{
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
