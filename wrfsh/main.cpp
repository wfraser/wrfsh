#include "unicodehack.h"

#include <vector>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <list>
#include <memory>

#include "common.h"
#include "global_state.h"
#include "repl.h"
#include "stream_ex.h"

using namespace std;

int real_main(int argc, char *argv[], char *envp[])
{
    int exitCode;
    try
    {
        global_state gs(argc, argv, envp);

        unique_ptr<istream_ex> in;
        ostream_ex out(&cout);
        ostream_ex err(&cerr);

        if (argc == 2)
        {
            in = make_unique<istream_ex>(ospath(argv[1]));
        }
        else
        {
            in = make_unique<istream_ex>(&cin);
        }

        exitCode = repl(*in.get(), out, err, gs);
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
