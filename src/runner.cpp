#include "../runner.hpp"

#include <windows.h>

#include <chrono>
#include <filesystem>

#include "../args.hpp"
#include "../logger.hpp"
#include "../monitor.hpp"

namespace fs = std::filesystem;
extern Args arguments;

using std::string;

int runScript(const string& cmd, bool monitoring) {
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    DWORD code = -1;

    MonitoringResult result;

    auto start = std::chrono::steady_clock::now();
    if (CreateProcessA(NULL, const_cast<char*>(cmd.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        if (monitoring) monitorProcess(pi.dwProcessId, result);

        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    if (monitoring) {
        logMessage(INFO, "Результаты мониторинга:", true);
        logMessageA(INFO, "    Время выполнения: " + std::to_string(duration) + " ms", true);
        logMessageA(INFO, "    CPU max:     " + std::to_string(result.cpuMax) + "%", true);
        logMessageA(INFO, "    CPU average: " + std::to_string(result.cpuAverage) + "%", true);
        logMessageA(INFO, "    RAM max:     " + std::to_string(result.ramMax) + " MB", true);
        logMessageA(INFO, "    RAM average: " + std::to_string(result.ramAverage) + " MB", true);
    }

    return static_cast<int>(code);
}

void run() {
    logMessage(INFO, "Папка сборки: " + arguments.buildFolder, false, "📂");
    if (!arguments.files.empty()) {
        logMessage(INFO, "Файлы сборки: ", false, "📚");
        for (auto& file : arguments.files) logMessageA(INFO, "   * " + file);
    }

    fs::create_directories(arguments.buildFolder);
    fs::path outputPath = fs::absolute(fs::path(arguments.buildFolder) / (arguments.name + ".exe"));
    string compiler = arguments.useGCC ? "gcc" : "g++";

    auto joinQuoted = [](const std::vector<string>& v, const string& pre = "") -> string {
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
        int ret = runScript(cmd, true);
        if (ret != 0)
            logMessage(FAULT, "Завершена с ошибкой (" + std::to_string(ret) + ")");
        else
            logMessage(INFO, "Успешное завершение", true, "⏹️");
    }
}
