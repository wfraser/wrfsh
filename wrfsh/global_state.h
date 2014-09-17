#pragma once

class global_state
{
public:
    bool interactive;
    bool error;
    bool exit;
    std::unordered_map<std::string, std::string> environment;
    std::unordered_map<std::string, std::string> local_vars;

    struct program_line
    {
        std::string number;
        std::string command;
        std::vector<std::string> args;

        program_line(std::string number, std::string command, std::vector<std::string> args) :
            number(number),
            command(command),
            args(args)
        {}

        program_line() :
            number(""),
            command(""),
            args({})
        {}
    };
    std::list<program_line> stored_program;

    struct if_state_vars
    {
        bool active;
    };
    std::vector<if_state_vars> if_state;

    global_state(int argc, const char * const argv [], const char * const env []);
    std::string lookup_var(std::string key);
    void let(std::string key, std::string value);

    static int program_line_comp(program_line& a, program_line& b);
};