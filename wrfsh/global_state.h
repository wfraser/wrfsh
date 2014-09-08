#pragma once

class global_state
{
public:
    bool interactive;
    bool error;
    std::unordered_map<std::string, std::string> environment;
    std::unordered_map<std::string, std::string> local_vars;

    global_state(const char * const env[]) :
        interactive(false),
        error(false),
        environment({}),
        local_vars({})
    {
        for (size_t i = 0; env[i] != nullptr; i++)
        {
            std::string key, value;
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

    std::string lookup_var(std::string key)
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

    void let(std::string key, std::string value)
    {
        auto pos = local_vars.insert({ key, value });
        if (!pos.second)
        {
            pos.first->second = value;
        }
    }
};