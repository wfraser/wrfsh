#include "unicodehack.h"

#include <iostream>
#include <string>
#include <vector>

#ifdef _MSC_VER
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <comdef.h>
#include <io.h>
#endif

#include "process.h"

using namespace std;

#ifdef _MSC_VER

HANDLE RunCommandWin32(
    const wstring& exePath,
    const wchar_t* cmdline,
    HANDLE in,
    HANDLE out,
    HANDLE err,
    ostream& err_stream
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
            err_stream << "Error: command not found: " << Narrow(cmd) << endl;
        }
        else
        {
            _com_error err(hr);
            err_stream << "CreateProcessW failed: " << Narrow(err.ErrorMessage()) << endl;
        }
        return INVALID_HANDLE_VALUE;
    }

    return pi.hProcess;
}

struct IOThreadArgs
{
    HANDLE childHandle;
    ios *stream;
    bool terminate;
};

DWORD WINAPI ReadThreadProc(LPVOID lpThreadParam)
{
    auto args = reinterpret_cast<IOThreadArgs*>(lpThreadParam);
    auto stream = dynamic_cast<ostream*>(args->stream);

    char readBuf[512];
    SetLastError(0);
    for (;;)
    {
        DWORD bytesRead;
        BOOL ok = ReadFile(args->childHandle, readBuf, ARRAYSIZE(readBuf), &bytesRead, nullptr);
        if (!ok || bytesRead == 0)
        {
            if (GetLastError() != ERROR_BROKEN_PIPE)
            {
                _com_error err(HRESULT_FROM_WIN32(GetLastError()));
                cerr << "ReadFile from pipe to child process failed: " << Narrow(err.ErrorMessage()) << endl;
            }
            break;
        }

        stream->write(readBuf, bytesRead);
    }

    return 0;
}

DWORD WINAPI WriteThreadProc(LPVOID lpThreadParam)
{
    auto args = reinterpret_cast<IOThreadArgs*>(lpThreadParam);
    auto stream = dynamic_cast<istream*>(args->stream);

    char writeBuf[512];
    for (;;)
    {
        DWORD bytesRead = 1;
        stream->read(writeBuf, 1);   // Block until a byte is available.
        if (stream->gcount() == 0)
            break;

        // Try to read some more while we're at it
        bytesRead += (DWORD)stream->readsome(writeBuf+1, ARRAYSIZE(writeBuf)-1);

        DWORD bytesWritten;
        BOOL ok = WriteFile(args->childHandle, writeBuf, bytesRead, &bytesWritten, nullptr);
        if (!ok || bytesWritten != bytesRead)
            break;
    }

    return 0;
}

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

#endif

Process::Process(const string program, const vector<string> args) :
    m_program(program),
    m_args(args)
{
#ifdef _MSC_VER
    // Commands implemented by cmd.exe
    if (m_program == "dir")
    {
        vector<string> new_args({ "/c", m_program });
        new_args.insert(new_args.end(), m_args.begin(), m_args.end());
        swap(m_args, new_args);
        m_program = "cmd";
    }
#endif
}

bool Process::Run(istream& in, ostream& out, ostream& err, int *pExitCode)
{
#ifdef _MSC_VER

    *pExitCode = -1;

    bool needs_io_thread = false;
    ManagedHandle hIn, hOut, hErr;
    ManagedHandle hThreadIn, hThreadOut, hThreadErr;

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    auto fn = [&needs_io_thread, &sa](ManagedHandle& childHandle, ManagedHandle& threadHandle, ManagedHandle& readHandle, ManagedHandle& writeHandle, ios& stream, ios& stdStream, DWORD handleName)
    {
        if (&stream == &stdStream, false)
        {
            childHandle = GetStdHandle(handleName);
            threadHandle = INVALID_HANDLE_VALUE;
        }
        else
        {
            needs_io_thread = true;
            CreatePipe(&readHandle, &writeHandle, &sa, 0);
            SetHandleInformation(childHandle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
            SetHandleInformation(threadHandle, HANDLE_FLAG_INHERIT, 0);
        }
    };

    //child  thread      read        write      str  std   handleName
    fn(hIn,  hThreadIn,  hIn,        hThreadIn, in,  cin,  STD_INPUT_HANDLE);
    fn(hOut, hThreadOut, hThreadOut, hOut,      out, cout, STD_OUTPUT_HANDLE);
    fn(hErr, hThreadErr, hThreadErr, hErr,      err, cerr, STD_ERROR_HANDLE);

    wstring wargs = Widen(m_program);
    for (auto it = m_args.begin(), end = m_args.end(); it != end; ++it)
    {
        wargs.push_back(L' ');
        wargs.append(Widen(*it));
    }

    ManagedHandle hProcess = RunCommandWin32(L"", wargs.c_str(), hIn, hOut, hErr, err);

    // Close the child's end of the pipes. It inherited its own handles for them.
    hIn.Close();
    hOut.Close();
    hErr.Close();

    if (hProcess == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    if (needs_io_thread)
    {
        IOThreadArgs //inArgs = { hThreadIn, &in },
            outArgs = { hThreadOut, &out },
            errArgs = { hThreadErr, &err };

        // TODO FIXME:
        // The stdin write thread is problematic because it uses blocking calls.
        // Once it blocks, it won't exit.
        //ManagedHandle hInThread = CreateThread(nullptr, 0, WriteThreadProc, &inArgs, 0, nullptr);
        ManagedHandle hOutThread = CreateThread(nullptr, 0, ReadThreadProc, &outArgs, 0, nullptr);
        ManagedHandle hErrThread = CreateThread(nullptr, 0, ReadThreadProc, &errArgs, 0, nullptr);

        if (WAIT_OBJECT_0 != WaitForSingleObject(hProcess, INFINITE))
        {
            _com_error error(HRESULT_FROM_WIN32(GetLastError()));
            err << "failed to wait on child process: " << Narrow(error.ErrorMessage()) << endl;
            return false;
        }

        HANDLE threads[] = { /*hInThread,*/ hOutThread, hErrThread };

        if (WAIT_OBJECT_0 != WaitForMultipleObjects(ARRAYSIZE(threads), threads, /*waitall:*/ true, INFINITE))
        {
            _com_error error(HRESULT_FROM_WIN32(GetLastError()));
            err << "failed to wait on I/O threads: " << Narrow(error.ErrorMessage()) << endl;
            return false;
        }
    }

    DWORD exitCode;
    GetExitCodeProcess(hProcess, &exitCode);
    *pExitCode = (int)exitCode;

    return true;

#else
#error posix support for Process::Run not implemented yet.
#endif
}