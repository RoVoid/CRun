#include <windows.h>

#include <filesystem>
#include <stdexcept>

#include "args.hpp"
#include "config.hpp"
#include "logger.hpp"
#include "runner.hpp"

#define VERSION "0.3 Alpha"

namespace fs = std::filesystem;

#ifdef _WIN32
#include <windows.h>

fs::path getExecutablePath() {
    wchar_t buffer[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (len == 0) throw std::runtime_error("GetModuleFileNameW failed");
    return fs::path(buffer).parent_path();
}
#else
#include <unistd.h>
fs::path getExecutablePath() {
    char buffer[4096];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len == -1) throw std::runtime_error("readlink(/proc/self/exe) failed");
    buffer[len] = '\0';
    return fs::path(buffer).parent_path();
}
#endif

int main(int argc, char* argv[]) {
    std::string script;

    if (argc >= 2) {
        std::string command = argv[1];
        if (command == "r" || command == "run") {
            if (argc < 3) {
                logMessage(FAULT, " ");
                return 1;
            }
            script = argv[2];
        }
        else if (command == "v" || command == "version") {
            logMessage(INFO, std::string("CRUN ") + VERSION, true, "🧠");
            return 0;
        }
        else if (command == "h" || command == "help") {
            logMessage(INFO, "CRUN — компилятор и запуск C/C++ проектов", true, "🛠️");

            logMessage(INFO, "Команды:", true, "📌");
            logMessageA(INFO, "    run <script>         — выполнить скрипт из crun.json", true);
            logMessageA(INFO, "    init                 — создать шаблон crun.json", true);
            logMessageA(INFO, "    version              — показать версию", true);
            logMessageA(INFO, "    help                 — показать эту справку", true);
            logMessageA(INFO, "    <files...> <options> — показать эту справку", true);

            logMessage(INFO, "Флаги:", true, "🏷️");
            logMessageA(INFO, "    -c, -clear        — очистить консоль перед запуском", true);
            logMessageA(INFO, "    -r, -run          — запуск после сборки", true);
            logMessageA(INFO, "    -b, -build        — только сборка", true);
            logMessageA(INFO, "    -gcc              — использовать gcc вместо g++", true);
            logMessageA(INFO, "    -g++              — использовать g++", true);
            logMessageA(INFO, "    -bd, -buildDir    — указать папку сборки", true);
            logMessageA(INFO, "    -I <dir>          — добавить include папку", true);
            logMessageA(INFO, "    -L <dir>          — добавить папку с библиотеками", true);
            logMessageA(INFO, "    -l <lib>          — добавить библиотеку", true);
            logMessageA(INFO, "    -F <folder>       — добавить папку с исходниками", true);  // убрать
            logMessageA(INFO, "    -f <file>         — добавить файл", true);                 // убрать
            logMessageA(INFO, "    -o <options...>   — дополнительные опции компилятора", true);
            logMessageA(INFO, "    -- <...>          — аргументы для исполняемого файла", true);
            return 0;
        }
    }

    fs::path localPath = fs::absolute("./.crun/config.json");

    if (readConfig(localPath.string(), script)) {
        fs::path globalPath = fs::absolute(getExecutablePath() / ".crun/config.json");
        if (!fs::equivalent(localPath, globalPath)) readConfig(globalPath.string(), script);
    }

    if (!arguments.scriptToRun.empty()) { return runScript(arguments.scriptToRun); }

    parseArgs(argc - 1, argv + 1);

    run();

    return 0;
}
