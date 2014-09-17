#include <string>
#include <vector>
#include <unordered_map>
#include <list>

#include "common.h"
#include "global_state.h"

using namespace std;

global_state::global_state(int argc, const char * const argv [], const char * const env []) :
    interactive(false),
    error(false),
    exit(false),
    environment({}),
    local_vars({}),
    stored_program({}),
    if_state({})
{
    for (int i = 0; i < argc; i++)
    {
        let(to_string(i), argv[i]);
    }
    
    let("#", to_string(argc));

    let("?", "0");

    for (size_t i = 0; env[i] != nullptr; i++)
    {
        string key, value;
        bool found_equals = false;
        for (size_t j = 0; env[i][j] != '\0'; j++)
        {
            if (env[i][j] == '=')
            {
                found_equals = true;
            }
            else if (!found_equals)
            {
                key.push_back(env[i][j]);
            }
            else
            {
                value.push_back(env[i][j]);
            }
        }
        environment.emplace(key, value);
    }
}

std::string global_state::lookup_var(string key)
{
    auto pos = local_vars.find(key);
    if (pos != local_vars.end())
    {
        return pos->second;
    }
    pos = environment.find(key);
    if (pos != environment.end())
    {
        return pos->second;
    }
    return "";
}

void global_state::let(string key, string value)
{
    auto pos = local_vars.insert({ key, value });
    if (!pos.second)
    {
        pos.first->second = value;
    }
}

int global_state::program_line_comp(global_state::program_line& a, global_state::program_line& b)
{
    return atoi(a.number.c_str()) < atoi(b.number.c_str());
}
