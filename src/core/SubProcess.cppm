module;

/* This Module is heavily dependent on Windows Platform and Windows API functions
 */
#include <string>
#include <vector>
#include <iostream>
#include <functional>
#include <windows.h>
#include <processthreadsapi.h>
#include <handleapi.h>

#define BUFSIZE 4096 

export module subprocess;

export enum SubProcessFlags : char {
    None = 1,
    CreateConsole = 2,
    RedirectOutput = 4,
};

export SubProcessFlags operator|(SubProcessFlags lhs, SubProcessFlags rhs)  {
    return static_cast<SubProcessFlags>(static_cast<char>(lhs) | static_cast<char>(rhs));
}

export SubProcessFlags operator&(SubProcessFlags lhs, SubProcessFlags rhs)  {
    return static_cast<SubProcessFlags>(static_cast<char>(lhs) & static_cast<char>(rhs));
}

export class SubProcess
{
public:
    PROCESS_INFORMATION pi;

    HANDLE g_hChildStd_OUT_Rd = NULL;
    HANDLE g_hChildStd_OUT_Wr = NULL;

    SubProcess() {
        ZeroMemory( &pi, sizeof(pi) );
        pi.hProcess = nullptr;
    }

    ~SubProcess() {
        if (isAlive()) {
            terminate();
        }
        cleanUp();
    }

    void cleanUp() {
        if (pi.hProcess != nullptr) {
            // Close process and thread handles.
            CloseHandle( pi.hProcess );
        }
        pi.hProcess = nullptr;

        if (pi.hThread != nullptr) {
            CloseHandle(pi.hThread);
        }
        pi.hThread = nullptr;
    }

    bool create(const std::string& cmd, SubProcessFlags flags) {
        STARTUPINFOA siStartInfo;
        ZeroMemory( &siStartInfo, sizeof(siStartInfo) );

        SECURITY_ATTRIBUTES saAttr;

        // Set the bInheritHandle flag so pipe handles are inherited.
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        bool redirectOutput = flags & SubProcessFlags::RedirectOutput;

        if (redirectOutput) {
            // Create a pipe for the child process's STDOUT. 
            if ( ! CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0) ) {
                printf("CRITICAL ERROR: Failed to create pipe for STDOUT\n");
                return false;
            }
            // Ensure the read handle to the pipe for STDOUT is not inherited
            if ( ! SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0) ){
                printf("CRITICAL ERROR: STDOUT is inherited\n");
                return false;
            }
        }

        siStartInfo.cb = sizeof(siStartInfo);

        siStartInfo.wShowWindow = flags & SubProcessFlags::CreateConsole ? SW_SHOW : SW_HIDE;

        siStartInfo.dwFlags = STARTF_USESHOWWINDOW;

        if(redirectOutput) {
            siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
            siStartInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
            siStartInfo.hStdError = g_hChildStd_OUT_Wr;
        }

        DWORD creationFlags = flags & SubProcessFlags::CreateConsole ? CREATE_NEW_CONSOLE : CREATE_NO_WINDOW;

        std::string argv = cmd;

        if( !CreateProcessA(nullptr,   // No module name (use command line)
            &argv[0],        // Command line
            nullptr,           // Process handle not inheritable
            nullptr,           // Thread handle not inheritable
            true,               // handles are inherited
            creationFlags,      // creation flags
            nullptr,           // Use parent's environment block
            nullptr,           // Use parent's starting directory
            &siStartInfo,                // Pointer to STARTUPINFO structure
            &pi )               // Pointer to PROCESS_INFORMATION structure
        )
        {
            return false;
        }

        if (redirectOutput) {
            CloseHandle(g_hChildStd_OUT_Wr);
        }
        return true;
    }

    void read(std::function<void(const std::string&)> cb) {
        DWORD dwRead; 
        CHAR chBuf[BUFSIZE]; 
        BOOL bSuccess = FALSE;

        for (;;)  {
            bSuccess = ReadFile(g_hChildStd_OUT_Rd, chBuf, BUFSIZE, &dwRead, NULL);
            if( ! bSuccess || dwRead == 0 ) 
                break; 

            std::string ss(chBuf, dwRead);
            cb(ss);
        } 
    }

    void terminate() {
        if (pi.hProcess == nullptr) {
            return;
        }
        TerminateProcess(pi.hProcess, 0);
    }

    bool isAlive() const {
        if (pi.hProcess == nullptr) {
            return false;
        }
        int exitCode;
        getExitCode(&exitCode);
        return exitCode == STILL_ACTIVE;
    }

    int getExitCode(int* pExitCode) const {
        if (pi.hProcess == nullptr) {
            return -1;
        }
        DWORD exitCode;
        int ret = GetExitCodeProcess(pi.hProcess, &exitCode);
        *pExitCode = exitCode;
        return ret;
    }

    DWORD wait() const {
        return WaitForSingleObject(pi.hProcess, INFINITE);
    }
};
