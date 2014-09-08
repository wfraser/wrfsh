#pragma once

class global_state
{
public:
    bool interactive;
    bool error;
    std::unordered_map<std::string, std::string> environment;
    std::unordered_map<std::string, std::string> local_vars;

    struct if_state_vars
    {
        bool active;
    };
    std::vector<if_state_vars> if_state;

    global_state(int argc, const char * const argv [], const char * const env []);
    std::string lookup_var(std::string key);
    void let(std::string key, std::string value);
};