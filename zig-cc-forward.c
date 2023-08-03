#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CMD_MAX_LEN (64 * 1024)
#define CMD_MAX_ARGS 4096

#if !defined(_WIN32)
#   include <unistd.h>
#else
#   include <windows.h>
#   include <shlwapi.h>
#endif

static const char* zigValidCmds[] = {
    "cc",
    "c++",
    "ar",
    "dlltool",
    "lib",
    "ranlib",
    "objcopy",
    NULL
};

static int strncmp_tolower(const char *a, const char *b, size_t n) {
    if (a && b) {
        for (size_t i = 0; i < n && a[i] && b[i]; ++i) {
            char lower_a = tolower(a[i]);
            char lower_b = tolower(b[i]);

            if (lower_a != lower_b) {
                return lower_a - lower_b;
            }
        }
    }
    else if (!a && b) {
        return -1;
    }
    else if (a && !b) {
        return 1;
    }
    else {
        return 0;
    }

    if (n > 0) {
        if (a[n - 1] == '\0' && b[n - 1] == '\0') {
            return 0;
        }

        if (a[n - 1] == '\0') {
            return -1;
        }

        if (b[n - 1] == '\0') {
            return 1;
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
    int err = EXIT_SUCCESS;
    
    if (argc < 2) {
        fprintf(stderr, "Usage:\n");
        for (int i = 0; zigValidCmds[i]; ++i) {
            fprintf(stderr, "  zig-%s <args>\n", zigValidCmds[i]);
        }
        return EXIT_FAILURE;
    }

    const char* zigExe = "zig";
    const char* zigCmd = "cc";

    const char* arg0 = argv[0];
    if (arg0) {
        const char* idxDash = strrchr (arg0, '-');
        const char* idxEnd = strrchr (arg0, '.');
        if (!idxEnd) {
            idxEnd = arg0 + strlen(arg0);
        }
        if (idxDash && idxDash >= arg0+3) {
            const char* s = idxDash - 3;
            if (strncmp_tolower(s, "zig", 3) == 0) {
                // fprintf(stderr, "zig-cc: matched 'zig-' binary prefix\n");
                const char** cmdIter = zigValidCmds;
                while (*cmdIter) {
                    const char* cmd = *cmdIter++;
                    int cmdLen = strlen(cmd);
                    if (idxDash+1+cmdLen == idxEnd && strncmp_tolower(idxDash+1, cmd, cmdLen) == 0) {
                        // fprintf(stderr, "zig-cc: matched zig command suffix: %s\n", cmd);
                        zigCmd = cmd;
                        break;
                    }
                }
            }
        }
        
    }

#if !defined(_WIN32)
    char *zig_args[CMD_MAX_ARGS];
    zig_args[0] = zigCmd;
    for (int i = 1; i < argc; ++i) {
        zig_args[i] = argv[i];
    }
    zig_args[argc] = NULL;
    
    if (execvp("zig", zig_args) != 0) {
        perror("failed to execute command");
        return EXIT_FAILURE;
    }
#else
    WCHAR zigExeW[MAX_PATH];
    WCHAR zigExeFullPath[MAX_PATH];

    // Convert binary name to wide character string
    int result = MultiByteToWideChar(CP_UTF8, 0, zigExe, -1, zigExeW, MAX_PATH);
    if (result == 0) {
        fprintf(stderr, "Error converting binary name to wide character. Error code: %d\n", GetLastError());
        return EXIT_FAILURE;
    }

    result = PathAddExtensionW(zigExeW, L".exe");
    if (result == 0) {
        fprintf(stderr, "Error adding extension to binary name. Error code: %d\n", GetLastError());
        return EXIT_FAILURE;
    }

    // Search for the full path
    result = SearchPathW(NULL, zigExeW, NULL, MAX_PATH, zigExeFullPath, NULL);
    if (result <= 0) {
        fwprintf(stderr, L"Could not find zig binary (%d): %ls\n", GetLastError(), zigExeW);
        return EXIT_FAILURE;
    }

    LPWSTR szCommandLine;
    int i;

    szCommandLine = GetCommandLineW();
    if (!szCommandLine) {
        fprintf(stderr, "GetCommandLineW failed (%d)\n", GetLastError());
        return EXIT_FAILURE;
    }

    LPWSTR szCommandArgs = NULL;
    if (szCommandLine[0] == L'"') {
        szCommandArgs = wcsstr(szCommandLine + 1, L"\" ");
        if (szCommandArgs) {
            szCommandArgs += 1;
        }
    }
    else {
        szCommandArgs = wcsstr(szCommandLine, L" ");
    }

    if (!szCommandArgs) {
        szCommandArgs = L"";
    }

    WCHAR commandLine[CMD_MAX_LEN];
    wsprintfW(commandLine, L"\"%ls\" %hs", zigExeFullPath, zigCmd);
    wcscat(commandLine, szCommandArgs);

    // wprintf(L"CMD: %ls\n", szCommandLine);
    // wprintf(L"ARGS: %ls\n", szCommandArgs);
    // wprintf(L"CALL: %ls\n", commandLine);

    // LPWSTR* szArglist;
    // int nArgs;
    // szArglist = CommandLineToArgvW(szCommandLine, &nArgs);
    // if( NULL == szArglist )
    // {
    //     fprintf(stderr, "CommandLineToArgvW failed (%d)\n", GetLastError());
    //     return EXIT_FAILURE;
    // }
    // else for( i=0; i<nArgs; i++) wprintf(L"%d: %ls\n", i, szArglist[i]);
    // LocalFree(szArglist);

    // Set up STARTUPINFO structure
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    // Create the process
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessW(
            NULL,                // No module name (use command line)
            commandLine,         // Command line
            NULL,                // Process handle not inheritable
            NULL,                // Thread handle not inheritable
            TRUE,                // Set handle inheritance to TRUE
            0,                   // No creation flags
            NULL,                // Use parent's environment block
            NULL,                // Use parent's starting directory 
            &si,                 // Pointer to STARTUPINFO structure
            &pi)                 // Pointer to PROCESS_INFORMATION structure
        ) {
        fprintf(stderr, "CreateProcess failed (%d)\n", GetLastError());
        return EXIT_FAILURE;
    }

    // Wait for the child process to exit
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Retrieve the exit code
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    //printf("Child process exited with code %d\n", exitCode);

    // Close process and thread handles
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    err = exitCode;
#endif

    return err;
}
