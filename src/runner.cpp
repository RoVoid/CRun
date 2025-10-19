#include "../runner.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <sstream>

#include "../args.hpp"
#include "../logger.hpp"
#include "../monitor.hpp"

namespace fs = std::filesystem;
extern Args arguments;

using std::string;

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

int runScript(const string& cmd, bool monitoring) {
    MonitoringResult result{};
    auto start = std::chrono::steady_clock::now();
    int code = -1;

#ifdef _WIN32
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    if (CreateProcessA(NULL, const_cast<char*>(cmd.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        if (monitoring) monitorProcess(pi.dwProcessId, result);

        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, (LPDWORD)&code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else {
        logMessage(FAULT, "Не удалось запустить процесс: " + cmd);
        return -1;
    }

#else  // Linux / macOS
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", cmd.c_str(), (char*)nullptr);
        _exit(127);  // если exec не сработал
    }
    else if (pid > 0) {
        if (monitoring) monitorProcess(pid, result);
        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) code = WEXITSTATUS(status);
        else code = -1;
    }
    else {
        logMessage(FAULT, "Не удалось запустить процесс: " + cmd);
        return -1;
    }
#endif

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    logMessageA(INFO, "", true);

    if (monitoring) {
        logMessage(INFO, "Результаты мониторинга:", true);
        logMessageA(INFO, "    Время выполнения: " + std::to_string(duration) + " ms", true);
        logMessageA(INFO, "    CPU max:     " + std::to_string(result.cpuMax) + "%", true);
        logMessageA(INFO, "    CPU average: " + std::to_string(result.cpuAverage) + "%", true);
        logMessageA(INFO, "    RAM max:     " + std::to_string(result.ramMax) + " MB", true);
        logMessageA(INFO, "    RAM average: " + std::to_string(result.ramAverage) + " MB", true);
    }

    return code;
}

void run() {
    if (arguments.name.empty()) {
        if (arguments.files.empty()) {
            arguments.files.insert(arguments.downToC ? "main.c" : "main.cpp");
            arguments.name = "main";
        }
        if (arguments.name.empty() && !arguments.files.empty()) {
            fs::path p(*arguments.files.begin());
            arguments.name = p.stem().string();
        }
    }

    if (arguments.clear) {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
    }

    logMessage(INFO, "Папка сборки: " + arguments.buildFolder, false, "📂");
    if (!arguments.files.empty()) {
        logMessage(INFO, "Файлы сборки: ", false, "📚");
        for (auto& file : arguments.files) logMessageA(INFO, "   * " + file);
    }

    fs::create_directories(arguments.buildFolder);

#ifdef _WIN32
    fs::path outputPath = fs::absolute(fs::path(arguments.buildFolder) / (arguments.name + ".exe"));
#else
    fs::path outputPath = fs::absolute(fs::path(arguments.buildFolder) / arguments.name);
#endif

    string compiler = arguments.downToC ? "gcc" : "g++";

    auto joinQuoted = [](const std::set<string>& v, const string& pre = "") -> string {
        std::ostringstream oss;
        for (auto& x : v) oss << ' ' << pre << '"' << x << '"';
        return oss.str();
    };

    string filesStr = joinQuoted(arguments.files);
    string includeStr = joinQuoted(arguments.includeDirs, "-I");
    string libDirStr = joinQuoted(arguments.libDirs, "-L");
    string libsStr;
    for (auto& lib : arguments.libsList) libsStr += " -l" + lib;

    if (arguments.launch != RUN) {
        std::ostringstream ss;
        ss << compiler << filesStr << libDirStr << includeStr << libsStr << " " << arguments.compilerOptions << " -o \""
           << outputPath.string() << "\" -finput-charset=UTF-8";
        logMessage(INFO, "Начало сборки " + arguments.name, true, "⚒️");
        if (system(ss.str().c_str()) != 0) {
            logMessage(FAULT, "Ошибка при компиляции!");
            return;
        }
        logMessage(INFO, "Сборка завершена", true, "✅");
    }

    if (arguments.launch != BUILD) {
        if (!fs::exists(outputPath)) {
            logMessage(FAULT, "Исполняемый файл не найден!", true, "❓");
            return;
        }

        string cmd = "\"" + outputPath.string() + "\" " + arguments.exeArgs;
        logMessage(INFO, "Запуск программы", true, "➡️");
        logMessageA(INFO, "", true);

        int ret = runScript(cmd, true);

        if (ret != 0) logMessage(FAULT, "Завершена с ошибкой (" + std::to_string(ret) + ")");
        else logMessage(INFO, "Успешное завершение", true, "⏹️");
    }
}
