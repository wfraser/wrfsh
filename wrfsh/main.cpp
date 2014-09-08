#include "unicodehack.h"

#include <vector>
#include <iostream>
#include <unordered_map>

#include "global_state.h"
#include "repl.h"

using namespace std;

int real_main(int argc, char *argv[], char *envp[])
{
    (void)(argc, argv, envp);

    int exitCode;
    try
    {
        //argc = 1;

        global_state gs(envp);

        if (argc == 2)
        {
            ifstream in(ospath(argv[1]), ios::in);
            exitCode = repl(in, cout, cerr, gs);
        }
        else
        {
            exitCode = repl(cin, cout, cerr, gs);
        }
    }
    catch (...)
    {
        cerr << "Exception caught from repl!\n";
        exitCode = -1;
    }

    return exitCode;
}

#ifdef _MSC_VER
int wmain(int argc, wchar_t *argv[], wchar_t *envp[])
{
    vector<char*> args;
    for (int i = 0; i < argc; i++)
    {
        args.push_back(_strdup(Narrow(argv[i]).c_str()));
    }

    vector<char*> env;
    for (size_t i = 0; envp[i] != nullptr; i++)
    {
        env.push_back(_strdup(Narrow(envp[i]).c_str()));
    }
    env.push_back(nullptr);

    int exitCode = real_main(argc, args.data(), env.data());

    for (char *str : args)
    {
        free(str);
    }

    for (char *str : env)
    {
        free(str);
    }

    return exitCode;
}
#else
int main(int argc, char *argv[], char *envp[])
{
    return real_main(argc, argv, envp);
}
#endif