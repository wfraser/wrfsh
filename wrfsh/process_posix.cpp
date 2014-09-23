#ifndef _MSC_VER

#include "unicodehack.h"

#include <iostream>
#include <string>
#include <vector>

#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
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
        ios& stream,
        ios& stdStream,
        int stdFd)
    {
        auto str_ex = dynamic_cast<stream_ex*>(&stream);
        if ((str_ex == nullptr) ? (&stream == &stdStream)
                                : (*str_ex == stdStream))
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
                writeFd = fd[0];
                readFd = fd[1];
            }
        }
    };

    // child  thread       read         write       stream  std   fd
    fn(fdIn,  fdThreadIn,  fdIn,        fdThreadIn, in,     cin,  0);
    fn(fdOut, fdThreadOut, fdThreadOut, fdOut,      out,    cout, 1);
    fn(fdErr, fdThreadErr, fdThreadErr, fdErr,      err,    cerr, 2);

    //TODO FIXME
    if (needs_io_thread)
    {
        fdIn.LeaveOpen();
        fdOut.LeaveOpen();
        fdErr.LeaveOpen();
        err << "error: redirected I/O for child processes not supported on Posix (yet).\n";
        return false;
    }

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
            dup2(fdIn,  0);
            dup2(fdOut, 1);
            dup2(fdErr, 2);

            fdIn.Close();
            fdOut.Close();
            fdErr.Close();

            fdThreadIn.Close();
            fdThreadOut.Close();
            fdThreadErr.Close();
        }

        vector<const char*> args;
        args.push_back(m_program.c_str());
        for (const auto& arg : m_args)
        {
            args.push_back(arg.c_str());
        }
        args.push_back(nullptr);

        execvp(m_program.c_str(), const_cast<char * const *>(&args[0]));
    }
    else
    {
        // Parent

        if (needs_io_thread)
        {
            // TODO FIXME: start threads
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
            // TODO FIXME: Wait on threads
        }

        return true;
    }

    return false;
}

#endif // _MSC_VER
