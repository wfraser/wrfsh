#pragma once

typedef int (*commandlet_function)(
    std::istream& in,
    std::ostream& out,
    std::ostream& err,
    global_state& gs,
    std::vector<std::string>& args
    );

#define COMMANDLET(name) int name##_commandlet( \
    std::istream& in, \
    std::ostream& out, \
    std::ostream& err, \
    global_state& gs, \
    std::vector<std::string>& args \
    )

COMMANDLET(let);
COMMANDLET(echo);
COMMANDLET(list);
COMMANDLET(run);
COMMANDLET(new);
COMMANDLET(if);
COMMANDLET(else);
COMMANDLET(endif);
COMMANDLET(exit);
COMMANDLET(cd);

#ifdef _MSC_VER
COMMANDLET(pwd);
#endif

#undef COMMANDLET

extern std::unordered_map<std::string, commandlet_function> special_functions;
