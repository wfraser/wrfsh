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
    DEFINE_COMMANDLET(if),      // defined in if_else_endif.cpp
    DEFINE_COMMANDLET(else),    //
    DEFINE_COMMANDLET(endif),   //
    DEFINE_COMMANDLET(echo),
    DEFINE_COMMANDLET(list),
});