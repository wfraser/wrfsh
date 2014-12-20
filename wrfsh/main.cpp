#include "unicodehack.h"

#include <vector>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <list>
#include <memory>
#include <sstream>
#include <streambuf>

#include "common.h"
#include "global_state.h"
#include "repl.h"
#include "stream_ex.h"
#include "console.h"

using namespace std;

int real_main(int argc, char *argv[], char *envp[])
{
    int exitCode;
    try
    {
        global_state gs(argc, argv, envp);

        ostream_ex out(&cout);
        ostream_ex err(&cerr);

        if (argc == 2)
        {
            istream_ex in(ospath(argv[1]));
            exitCode = repl(in, out, err, gs, cin);
        }
        else
        {
            gs.interactive = true;

            stringstream buffer;
            istream_ex in(&buffer);

            unique_ptr<Console> con(Console::Make());

            for (;;)
            {
                con->Prompt();
                string line = con->GetInput();
                in.clear();
                buffer << line;
                exitCode = repl(in, con->ostream(), con->ostream(), gs, cin);

                if (gs.exit)
                {
                    break;
                }
            }
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
