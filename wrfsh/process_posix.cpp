#ifndef _MSC_VER

#include "unicodehack.h"

#include <iostream>
#include <string>
#include <vector>
#include <typeinfo>

#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>

#include "stream_ex.h"
#include "process.h"

using namespace std;

class ManagedHandle
{
public:
    ManagedHandle() : m_fd(-1), m_closed(false)
    {
    }

    ManagedHandle(int fd) : m_fd(fd), m_closed(false)
    {
    }

    void Close()
    {
        if (!m_closed && m_fd != -1)
        {
            close(m_fd);
            m_closed = true;
        }
    }

    void LeaveOpen()
    {
        m_closed = true;
    }

    ManagedHandle& operator=(int fd)
    {
        Close();
        m_fd = fd;
        m_closed = false;
        return *this;
    }

    ~ManagedHandle()
    {
        Close();
    }

    operator int()
    {
        return m_fd;
    }

    int* operator&()
    {
        return &m_fd;
    }

private:
    int  m_fd;
    bool m_closed;
};

struct ReadThreadArgs
{
    ManagedHandle* childHandle; // For talking with the child process
    ostream* stream; // For talking with the shell
};

struct WriteThreadArgs
{
    ManagedHandle* childHandle;
    istream* stream;
};

void* ReadThreadProc(void *param)
{
    auto args = reinterpret_cast<ReadThreadArgs*>(param);
    ostream* out = args->stream;

    char readBuf[512];
    for (;;)
    {
        ssize_t bytesRead = read(*(args->childHandle), readBuf, 512);
        if (bytesRead == -1)
        {
            cerr << "read error: " << strerror(errno) << endl;
            break;
        }

        if (bytesRead == 0)
        {
            break;
        }

        out->write(readBuf, bytesRead);
    }

    return nullptr;
}

void* WriteThreadProc(void* param)
{
    auto args = reinterpret_cast<WriteThreadArgs*>(param);
    
    //TODO FIXME
    (void) args;
    cout << "hello from write proc\n";

    return nullptr;
}

bool Process::Run_Posix(istream& in, ostream& out, ostream& err, int *pExitCode)
{
    *pExitCode = -1;

    ManagedHandle fdIn, fdOut, fdErr;
    ManagedHandle fdThreadIn, fdThreadOut, fdThreadErr;
    bool needs_io_thread = false;

    auto fn = [&needs_io_thread](
        ManagedHandle& childFd,
        ManagedHandle& threadFd,
        ManagedHandle& readFd,
        ManagedHandle& writeFd,
        ios* stream,
        ios* stdStream,
        int stdFd)
    {
        auto str_ex = dynamic_cast<stream_ex*>(stream);
        if ((str_ex == nullptr) ? (stream == stdStream)
                                : (*str_ex == *stdStream))
        {
            // It's a standard stream, or a stream_ex wrapping a standard stream.
            childFd = stdFd;
            childFd.LeaveOpen();
            threadFd = -1;
        }
        else
        {
            if (str_ex != nullptr)
            {
                // It's a stream_ex wrapping some file handle.
                childFd = str_ex->get_native_handle();
                childFd.LeaveOpen();
                threadFd = -1;
                assert(childFd != -1);
            }
            else
            {
                // It's some other kind of stream.
                needs_io_thread = true;
                int fd[2];
                pipe(fd);
                readFd = fd[0];
                writeFd = fd[1];
            }
        }
    };

    // child  thread       read         write       stream  std    fd
    fn(fdIn,  fdThreadIn,  fdIn,        fdThreadIn, &in,    &cin,  0);
    fn(fdOut, fdThreadOut, fdThreadOut, fdOut,      &out,   &cout, 1);
    fn(fdErr, fdThreadErr, fdThreadErr, fdErr,      &err,   &cerr, 2);

    pid_t pid = fork();
    if (pid == -1)
    {
        err << "fork failed (" << errno << ") in running child process\n";
        return false;
    }
    else if (pid == 0)
    {
        // Child

        if (needs_io_thread)
        {
            auto prepareFDs = [](
                ManagedHandle& myFd,
                ManagedHandle& threadFd,
                int standardFd
                )
            {
                if (threadFd != -1)
                {
                    dup2(myFd, standardFd);
                    myFd.Close();
                    threadFd.Close();
                }
            };

            prepareFDs(fdIn, fdThreadIn, 0);
            prepareFDs(fdOut, fdThreadOut, 1);
            prepareFDs(fdErr, fdThreadErr, 2);
        }

        vector<const char*> args;
        args.push_back(m_program.c_str());
        for (const string& arg : m_args)
        {
            args.push_back(arg.c_str());
        }
        args.push_back(nullptr);

        execvp(m_program.c_str(), const_cast<char * const *>(&args[0]));
    }
    else
    {
        // Parent

        vector<pthread_t> threads;
        pthread_t inThread, outThread, errThread;

        WriteThreadArgs inThreadArgs;
        ReadThreadArgs outThreadArgs, errThreadArgs;

        if (needs_io_thread)
        {
            // Start threads

            auto createThread = [&threads](
                auto* args,
                pthread_t& thread,
                ManagedHandle& fdThread,
                ManagedHandle& fdChild,
                auto* stream,
                void*(*threadProc)(void*)
                )
            {
                if (fdThread != -1)
                {
                    fdChild.Close();
                    args->childHandle = addressof(fdThread);
                    args->stream = stream;
                    int result = pthread_create(&thread, nullptr, threadProc, args);
                    if (result != 0)
                    {
                        cerr << "failed to create thread: " << strerror(errno) << endl;
                    }
                    threads.push_back(thread);
                }
            };

            createThread(&inThreadArgs, inThread, fdThreadIn, fdIn, &in, WriteThreadProc);
            createThread(&outThreadArgs, outThread, fdThreadOut, fdOut, &out, ReadThreadProc);
            createThread(&errThreadArgs, errThread, fdThreadErr, fdErr, &err, ReadThreadProc);
        }
        else
        {
            fdIn.LeaveOpen();
            fdOut.LeaveOpen();
            fdErr.LeaveOpen();
        }

        waitpid(pid, pExitCode, 0);

        if (needs_io_thread)
        {
            for (pthread_t thread : threads)
            {
                pthread_join(thread, /* retval: */ nullptr);
            }
        }

        return true;
    }

    return false;
}

#endif // _MSC_VER
