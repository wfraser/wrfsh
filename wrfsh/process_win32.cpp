#ifdef _MSC_VER

#include "unicodehack.h"

#include <iostream>
#include <string>
#include <vector>
#include <sstream>

#include "common.h"
#include "stream_ex.h"
#include "process.h"

using namespace std;

class ManagedHandle
{
public:
    ManagedHandle() : m_handle(INVALID_HANDLE_VALUE), m_closed(false)
    {
    }

    ManagedHandle(HANDLE h) : m_handle(h), m_closed(false)
    {
    }

    void Close()
    {
        if (!m_closed && m_handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_handle);
            m_closed = true;
        }
    }

    void LeaveOpen()
    {
        m_closed = true;
    }

    ManagedHandle& operator=(HANDLE h)
    {
        Close();
        m_handle = h;
        m_closed = false;
        return *this;
    }

    ~ManagedHandle()
    {
        Close();
    }

    operator HANDLE()
    {
        return m_handle;
    }

    HANDLE* operator&()
    {
        return &m_handle;
    }

private:
    HANDLE m_handle;
    bool   m_closed;
};

HANDLE RunCommandWin32(
    const wstring& exePath,
    const wchar_t* cmdline,
    HANDLE in,
    HANDLE out,
    HANDLE err
    )
{
    wstring cmdline_copy(cmdline);

    STARTUPINFO si = {};
    si.cb = sizeof(STARTUPINFO);

    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = in;
    si.hStdOutput = out;
    si.hStdError = err;

    PROCESS_INFORMATION pi = {};

    BOOL result = CreateProcessW(
        exePath.empty() ? nullptr : exePath.data(),
        &cmdline_copy[0],   // note: this may be modified by the function!
        nullptr,            // process attributes: don't inherit process handle
        nullptr,            // thread attributes: don't inherit thread handle
        true,               // inherit handles
        CREATE_UNICODE_ENVIRONMENT, // creation flags
        nullptr,            // environment: use parent's environment
        nullptr,            // current directory: use parent's current directory
        &si,                // startup info
        &pi                 // process info
        );

    CloseHandle(pi.hThread);

    if (!result)
    {
        HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
        if (hr == HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE))
        {
            wstring cmd(cmdline);
            cmd = cmd.substr(0, cmd.find(' '));
            cerr << "Error: command not found: " << Narrow(cmd) << endl;
        }
        else
        {
            _com_error error(hr);
            cerr << "CreateProcessW failed: " << Narrow(error.ErrorMessage()) << endl;
        }
        return INVALID_HANDLE_VALUE;
    }

    return pi.hProcess;
}

struct IOThreadArgs
{
    ManagedHandle* childHandle;  // For talking with the child process
    ios* stream;                // For talking with the outside world
};

DWORD WINAPI ReadThreadProc(LPVOID lpThreadParam)
{
    auto args = reinterpret_cast<IOThreadArgs*>(lpThreadParam);
    auto out = dynamic_cast<ostream*>(args->stream);
    if (out == nullptr)
    {
        assert(false);
        cerr << "BUG: ReadThreadProc needs to be given an ostream.\n";
        return 1;
    }

    char readBuf[512];
    SetLastError(0);
    for (;;)
    {
        DWORD bytesRead;
        BOOL ok = ReadFile(*(args->childHandle), readBuf, ARRAYSIZE(readBuf), &bytesRead, nullptr);
        if (!ok || bytesRead == 0)
        {
            if (GetLastError() != ERROR_BROKEN_PIPE)
            {
                _com_error err(HRESULT_FROM_WIN32(GetLastError()));
                cerr << "ReadFile from pipe to child process failed: " << Narrow(err.ErrorMessage()) << endl;
            }
            break;
        }

        out->write(readBuf, bytesRead);
    }

    return 0;
}

DWORD WINAPI WriteThreadProc(LPVOID lpThreadParam)
{
    auto args = reinterpret_cast<IOThreadArgs*>(lpThreadParam);
    auto sstr = dynamic_cast<stringstream*>(args->stream);
    if (sstr != nullptr)
    {
        // Special case: input is from a string

        DWORD bytesWritten = 0;
        for (DWORD i = 0, n = (DWORD)sstr->str().size(); i < n; i += bytesWritten)
        {
            BOOL ok = WriteFile(*(args->childHandle), sstr->str().data(), n, &bytesWritten, nullptr);
            if (!ok)
            {
                break;
            }
        }

        args->childHandle->Close();
    }
    else
    {
        auto s_ex = dynamic_cast<stream_ex*>(args->stream);
        if (s_ex == nullptr)
        {
            assert(false);
            cerr << "BUG: WriteThreadProc needs to be given a stringstream or a stream_ex.\n";
            return 1;
        }

        char writeBuf[512];
        for (;;)
        {
            DWORD bytesRead = 1;

            // Blocks until a byte is available or is cancelled.
            DWORD result = ReadFile(s_ex->get_native_handle(), writeBuf, ARRAYSIZE(writeBuf), &bytesRead, nullptr);

            if (result != TRUE)
            {
                break;
            }

            if (bytesRead > 0)
            {
                DWORD bytesWritten;
                BOOL ok = WriteFile(args->childHandle, writeBuf, bytesRead, &bytesWritten, nullptr);
                if (!ok || bytesWritten != bytesRead)
                    break;
            }
        }
    }

    return 0;
}

bool Process::Run_Win32(istream& in, ostream& out, ostream& err, int *pExitCode)
{
    *pExitCode = -1;

    bool needs_io_thread = false;
    ManagedHandle hIn, hOut, hErr;
    ManagedHandle hThreadIn, hThreadOut, hThreadErr;

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    auto fn = [&needs_io_thread, &sa](
        ManagedHandle& childHandle,
        ManagedHandle& threadHandle,
        ManagedHandle& readHandle,
        ManagedHandle& writeHandle,
        ios& stream,
        ios& stdStream,
        DWORD handleName
        )
    {
        auto str_ex = dynamic_cast<stream_ex*>(&stream);
        if ((str_ex == nullptr) ? (&stream == &stdStream)
                                : (*str_ex == stdStream))
        {
            // It's a standard stream, or a stream_ex wrapping a standard stream.
            childHandle = GetStdHandle(handleName);
            childHandle.LeaveOpen();
            threadHandle = INVALID_HANDLE_VALUE;
        }
        else
        {
            if (str_ex != nullptr)
            {
                // It's a stream_ex wrapping some file handle.
                childHandle = str_ex->get_native_handle();
                childHandle.LeaveOpen();
                threadHandle = INVALID_HANDLE_VALUE;
                assert(childHandle != INVALID_HANDLE_VALUE);
            }
            else
            {
                // It's some other kind of iostream.
                // NOTE: The read thread only supports stringstreams here (or one of the two above)!
                // For the write thread, it can be any type of iostream.

                needs_io_thread = true;
                CreatePipe(&readHandle, &writeHandle, &sa, 0);
                SetHandleInformation(childHandle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
                SetHandleInformation(threadHandle, HANDLE_FLAG_INHERIT, 0);
            }
        }
    };

    //child  thread      read        write      stream  std   handleName
    fn(hIn,  hThreadIn,  hIn,        hThreadIn, in,     cin,  STD_INPUT_HANDLE);
    fn(hOut, hThreadOut, hThreadOut, hOut,      out,    cout, STD_OUTPUT_HANDLE);
    fn(hErr, hThreadErr, hThreadErr, hErr,      err,    cerr, STD_ERROR_HANDLE);

    wstring wargs = Widen(m_program);
    for (auto it = m_args.begin(), end = m_args.end(); it != end; ++it)
    {
        wargs.push_back(L' ');
        wargs.append(Widen(*it));
    }

    ManagedHandle hProcess = RunCommandWin32(L"", wargs.c_str(), hIn, hOut, hErr);

    if (hProcess == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    // Close the child's end of the pipes. It inherited its own handles for them.
    hIn.Close();
    hOut.Close();
    hErr.Close();

    if (needs_io_thread)
    {
        vector<HANDLE> threads;
        ManagedHandle hInThread, hOutThread, hErrThread;

        if (hThreadIn != INVALID_HANDLE_VALUE)
        {
            IOThreadArgs inArgs = { addressof(hThreadIn), &in };
            hInThread = CreateThread(nullptr, 0, WriteThreadProc, &inArgs, 0, nullptr);
            threads.push_back(hInThread);
        }
        if (hThreadOut != INVALID_HANDLE_VALUE)
        {
            IOThreadArgs outArgs = { addressof(hThreadOut), &out };
            hOutThread = CreateThread(nullptr, 0, ReadThreadProc, &outArgs, 0, nullptr);
            threads.push_back(hOutThread);
        }
        if (hThreadErr != INVALID_HANDLE_VALUE)
        {
            IOThreadArgs errArgs = { addressof(hThreadErr), &err };
            hErrThread = CreateThread(nullptr, 0, ReadThreadProc, &errArgs, 0, nullptr);
            threads.push_back(hErrThread);
        }

        if (WAIT_OBJECT_0 != WaitForSingleObject(hProcess, INFINITE))
        {
            _com_error error(HRESULT_FROM_WIN32(GetLastError()));
            cerr << "failed to wait on child process: " << Narrow(error.ErrorMessage()) << endl;
            return false;
        }

        // The input thread uses I/O that could block even after the process ends (i.e. waiting on terminal).
        // Cancel it so the thread can exit.
        CancelSynchronousIo(hInThread);

        if (WAIT_OBJECT_0 != WaitForMultipleObjects((DWORD)threads.size(), threads.data(), /*waitall:*/ true, INFINITE))
        {
            _com_error error(HRESULT_FROM_WIN32(GetLastError()));
            cerr << "failed to wait on I/O threads: " << Narrow(error.ErrorMessage()) << endl;
            return false;
        }
    }

    // Windows cmd.exe adds an extra newline after program output.
    // We should too.
    out << endl;

    DWORD exitCode;
    GetExitCodeProcess(hProcess, &exitCode);
    *pExitCode = (int)exitCode;

    return true;
}

#endif // _MSC_VER