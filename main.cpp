#include <windows.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "shlwapi.lib")

class JarLauncher {
private:
    std::wstring jdk_path;
    std::wstring jre_path;
    std::wstring jar_path;
    bool useJDK = false;
    bool environmentSpecified = false;
    std::vector<std::wstring> java_args;
    std::vector<std::wstring> app_args;
    bool collecting_app_args = false;

    std::wstring processConfigPath(const std::wstring& path) {
        if(path.empty()) return path;

        std::wstring processed = path;

        while (!processed.empty() && processed.front() == L' ')
            processed.erase(0, 1);
        while (!processed.empty() && processed.back() == L' ')
            processed.pop_back();

        if (processed.size() >= 2 && processed.front() == L'"' && processed.back() == L'"') {
            processed = processed.substr(1, processed.size() - 2);
        }

        if (!processed.empty() && processed.find(L"java.exe") == std::wstring::npos) {
            if (processed.back() != L'\\') processed += L'\\';
            processed += L"bin\\java.exe";
        }

        return processed;
    }

    std::wstring processQuotedArg(const std::wstring& arg) {
        if(arg.empty()) return arg;

        std::wstring processed = arg;
        if (processed.size() >= 2 && processed[0] == L'"' && processed[processed.size()-1] == L'"') {
            processed = processed.substr(1, processed.size() - 2);
        }
        return processed;
    }

    void processArgsString(const std::wstring& args) {
        if(args.empty()) return;

        std::wstring current;
        bool inQuotes = false;

        for (size_t i = 0; i < args.length(); i++) {
            wchar_t c = args[i];

            if (c == L'"') {
                inQuotes = !inQuotes;
                current += c;
            }
            else if (c == L' ' && !inQuotes) {
                if (!current.empty()) {
                    java_args.push_back(processQuotedArg(current));
                    current.clear();
                }
            }
            else {
                current += c;
            }
        }

        if (!current.empty()) {
            java_args.push_back(processQuotedArg(current));
        }
    }

    std::wstring getExecutablePath() {
        wchar_t buffer[MAX_PATH];
        if (GetModuleFileNameW(NULL, buffer, MAX_PATH) == 0) {
            return L"";
        }
        std::wstring path(buffer);
        size_t pos = path.find_last_of(L"\\/");
        return pos != std::wstring::npos ? path.substr(0, pos) : path;
    }

    bool loadConfig() {
        std::wstring configPath = getExecutablePath() + L"\\config.ini";

        if (!PathFileExistsW(configPath.c_str())) {
            return false;
        }

        wchar_t buffer[MAX_PATH];

        if (GetPrivateProfileStringW(L"Java", L"JDK_PATH", L"", buffer, MAX_PATH, configPath.c_str()) > 0) {
            jdk_path = processConfigPath(buffer);
        }

        if (GetPrivateProfileStringW(L"Java", L"JRE_PATH", L"", buffer, MAX_PATH, configPath.c_str()) > 0) {
            jre_path = processConfigPath(buffer);
        }

        return !jdk_path.empty() && !jre_path.empty() &&
               PathFileExistsW(jdk_path.c_str()) && PathFileExistsW(jre_path.c_str());
    }

    void saveConfig() {
        std::wstring configPath = getExecutablePath() + L"\\config.ini";

        size_t jdkPos = jdk_path.find(L"\\bin\\java.exe");
        size_t jrePos = jre_path.find(L"\\bin\\java.exe");

        std::wstring jdkPathToSave = L"\"" + (jdkPos != std::wstring::npos ?
            jdk_path.substr(0, jdkPos) : jdk_path) + L"\"";
        std::wstring jrePathToSave = L"\"" + (jrePos != std::wstring::npos ?
            jre_path.substr(0, jrePos) : jre_path) + L"\"";

        WritePrivateProfileStringW(L"Java", L"JDK_PATH", jdkPathToSave.c_str(), configPath.c_str());
        WritePrivateProfileStringW(L"Java", L"JRE_PATH", jrePathToSave.c_str(), configPath.c_str());
    }

    void parseCommandLine(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; i++) {
            if(!argv[i]) continue;

            std::wstring arg = argv[i];

            if (arg == L"-jdk") {
                useJDK = true;
                environmentSpecified = true;
            }
            else if (arg == L"-jre") {
                useJDK = false;
                environmentSpecified = true;
            }
            else if (arg == L"-args" && i + 1 < argc && argv[i + 1]) {
                processArgsString(argv[++i]);
            }
            else if (arg.find(L".jar") != std::wstring::npos) {
                jar_path = arg;
                collecting_app_args = true;
            }
            else if (collecting_app_args) {
                app_args.push_back(processQuotedArg(arg));
            }
            else if (arg.length() >= 2 && (arg.substr(0, 2) == L"-X" || arg.substr(0, 2) == L"-D")) {
                java_args.push_back(processQuotedArg(arg));
            }
        }
    }

public:
    JarLauncher() : useJDK(false), environmentSpecified(false), collecting_app_args(false) {
        if (!loadConfig()) {
            wchar_t buffer[MAX_PATH];
            if (GetEnvironmentVariableW(L"JAVA_HOME", buffer, MAX_PATH)) {
                jdk_path = processConfigPath(std::wstring(buffer));
                std::wstring base_path = std::wstring(buffer);
                size_t lastSlash = base_path.find_last_of(L"\\/");
                if (lastSlash != std::wstring::npos) {
                    std::wstring jre_test = base_path.substr(0, lastSlash) + L"\\jre";
                    if (PathFileExistsW(jre_test.c_str())) {
                        jre_path = processConfigPath(jre_test);
                    }
                }
            }

            if (jdk_path.empty()) {
                jdk_path = L"C:\\Program Files\\Java\\jdk1.8.0_xxx\\bin\\java.exe";
            }
            if (jre_path.empty()) {
                jre_path = L"C:\\Program Files\\Java\\jre1.8.0_xxx\\bin\\java.exe";
            }

            saveConfig();
        }
    }

    void init(int argc, wchar_t* argv[]) {
        if(argc > 0 && argv != nullptr) {
            parseCommandLine(argc, argv);
        }
    }

    bool launchJar(const std::wstring& jarPath) {
        if (!PathFileExistsW(jarPath.c_str())) {
            MessageBoxW(NULL, L"JAR文件不存在", L"错误", MB_OK | MB_ICONERROR);
            return false;
        }

        if (!environmentSpecified) {
            int result = MessageBoxW(
                NULL,
                L"选择Java运行环境:\n是 - JDK\n否 - JRE",
                L"环境选择",
                MB_YESNO | MB_ICONQUESTION
            );
            useJDK = (result == IDYES);
        }

        std::wstring javaPath = useJDK ? jdk_path : jre_path;
        if(javaPath.empty()) {
            MessageBoxW(NULL, L"Java路径无效", L"错误", MB_OK | MB_ICONERROR);
            return false;
        }

        // 构建命令行
        std::wstring cmdLine;

        // 添加清空控制台的命令
        if (!environmentSpecified) {
            cmdLine = L"\"" + javaPath + L"\" ";
        } else {
            cmdLine = L"cmd.exe /c cls && \"" + javaPath + L"\" ";
        }

        for (const auto& arg : java_args) {
            if(!arg.empty()) {
                if (arg.find(L' ') != std::wstring::npos && arg[0] != L'"') {
                    cmdLine += L"\"" + arg + L"\" ";
                } else {
                    cmdLine += arg + L" ";
                }
            }
        }

        cmdLine += L"-jar \"" + jarPath + L"\" ";

        for (const auto& arg : app_args) {
            if(!arg.empty()) {
                if (arg.find(L' ') != std::wstring::npos && arg[0] != L'"') {
                    cmdLine += L"\"" + arg + L"\" ";
                } else {
                    cmdLine += arg + L" ";
                }
            }
        }

        std::vector<wchar_t> cmdLineBuf(cmdLine.begin(), cmdLine.end());
        cmdLineBuf.push_back(0);

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;

        DWORD creationFlags = environmentSpecified ? CREATE_NEW_CONSOLE : CREATE_NO_WINDOW;

        if (!CreateProcessW(
            NULL,
            cmdLineBuf.data(),
            NULL,
            NULL,
            FALSE,
            creationFlags,
            NULL,
            NULL,
            &si,
            &pi
        )) {
            std::wstring errorMsg = L"启动失败\n命令行: " + cmdLine;
            MessageBoxW(NULL, errorMsg.c_str(), L"错误", MB_OK | MB_ICONERROR);
            return false;
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    int nArgs;
    LPWSTR* szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
    if (szArglist == NULL) {
        MessageBoxW(NULL, L"无法解析命令行参数", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    JarLauncher launcher;
    launcher.init(nArgs, szArglist);

    bool foundJar = false;
    for (int i = 1; i < nArgs; i++) {
        if(szArglist[i] && std::wstring(szArglist[i]).find(L".jar") != std::wstring::npos) {
            foundJar = launcher.launchJar(szArglist[i]);
            break;
        }
    }

    LocalFree(szArglist);

    if (!foundJar) {
        MessageBoxW(NULL, L"请指定JAR文件路径", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    return 0;
}
