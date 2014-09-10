#include <iostream>
#include <vector>
#include <list>
#include <unordered_map>
#include <string>
#include <memory>
#include <algorithm>
#include <functional>

#include "common.h"
#include "global_state.h"
#include "commandlets.h"

using namespace std;

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

        Expression() : type(Type::Empty)
        {}
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

    Expression root_expression = {};
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

            if (arg == "(")
            {
                stack.back()->type = Expression::Type::CompoundExpression;
                stack.back()->compound_expression = make_unique<CompoundExpression>();
                stack.back()->compound_expression->expr1 = make_unique<Expression>();
                stack.push_back(stack.back()->compound_expression->expr1.get());
            }
            else if (arg == ")")
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

            if (arg == ")")
            {
                stack.pop_back();
                if (stack.size() > 0)
                {
                    if (stack.back()->type == Expression::Type::CompoundExpression
                        && stack.back()->compound_expression->expr1 != nullptr
                        && stack.back()->compound_expression->expr2 == nullptr)
                    {
                        s = State::Expression2;
                    }
                    else
                    {
                        stack.pop_back();
                        s = State::Expression2;
                    }
                }
            }
            else if (find(logic_operators.begin(), logic_operators.end(), arg) != logic_operators.end())
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
                    stack.back()->compound_expression->op = arg;
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
    printAST(out, root_expression, 0);

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