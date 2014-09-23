#pragma once

class global_state;

int repl(
    std::istream& in,
    std::ostream& out,
    std::ostream& err,
    global_state& global_state
    );

std::string process_expression(
    const std::string& expression,
    global_state& global_state,
    std::istream& in,
    std::ostream& err
    );