#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <list>
#include <sstream>

#include "common.h"
#include "process.h"
#include "global_state.h"

using namespace std;

global_state::global_state(int argc, const char * const argv [], const char * const env []) :
    interactive(false),
    error(false),
    exit(false)
{
    for (int i = 0; i < argc; i++)
    {
        let(to_string(i), argv[i]);
    }

    let("#", to_string(argc - 1));
    let("?", "0");
    let("PWD", get_current_working_directory(cerr));

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


#ifdef _MSC_VER
    string user;
    string domain = lookup_var("USERDOMAIN");
    if (!domain.empty())
    {
        user = domain;
        user.push_back('\\');
    }
    user.append(lookup_var("USERNAME"));
    let("USER", user);
#endif

    if (environment.find("HOST") == environment.end())
    {
        vector<string> empty_args;
        Process hostname_process("hostname", empty_args);

        stringstream hostname_in, hostname_out, hostname_err;
        int exitCode;
        if (hostname_process.Run(hostname_in, hostname_out, hostname_err, &exitCode))
        {
            string hostname = hostname_out.str();
            let("HOST", hostname.substr(0, hostname.find_last_of("\r\n")));
        }
        else
        {
            let("HOST", "localhost");
        }
    }
}

std::string global_state::lookup_var(string key)
{
    // Special variables:
    if (key == "*")
    {
        string result;

        int n = atoi(local_vars["#"].c_str());
        for (int i = 0; i < n; i++)
        {
            result += ((i != 0) ? " " : "") + local_vars[to_string(i+1)];
        }

        return result;
    }
    else if (key == "$")
    {
#ifdef _MSC_VER
        return to_string(GetCurrentProcessId());
#else
        return to_string(getpid());
#endif
    }

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
