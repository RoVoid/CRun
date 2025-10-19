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

    setupConsoleUTF8();

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
            logMessageA(INFO, "    run <script>         — выполнить из crun.yaml", true);
            logMessageA(INFO, "    init                 — создать шаблон crun.yaml", true);
            logMessageA(INFO, "    version              — показать версию", true);
            logMessageA(INFO, "    help                 — показать эту справку", true);
            logMessageA(INFO, "    <...>                — выполнение", true);

            logMessage(INFO, "Флаги:", true, "🏷️");
            logMessageA(INFO, "    -c, -clear        — очистить консоль перед запуском", true);
            logMessageA(INFO, "    -r, -run          — запуск после сборки", true);
            logMessageA(INFO, "    -b, -build        — только сборка", true);
            logMessageA(INFO, "    -n                — установить имя", true);
            logMessageA(INFO, "    -gcc              — использовать gcc вместо g++", true);
            logMessageA(INFO, "    -g++              — использовать g++", true);
            logMessageA(INFO, "    -bd, -buildDir    — указать папку сборки", true);
            logMessageA(INFO, "    -i <dir>          — добавить include папку (.h | .hpp)", true);
            logMessageA(INFO, "    -l <dir>          — добавить папку с библиотеками", true);
            logMessageA(INFO, "    -l <lib>          — добавить библиотеку", true);
            logMessageA(INFO, "    -o <options...>   — дополнительные опции компилятора", true);
            logMessageA(INFO, "    -- <...>          — аргументы для исполняемого файла", true);
            return 0;
        }
    }

    fs::path localYml = fs::absolute("./.crun/config.yml");
    fs::path localYaml = fs::absolute("./.crun/config.yaml");
    fs::path globalYml = fs::absolute(getExecutablePath() / ".crun/config.yml");
    fs::path globalYaml = fs::absolute(getExecutablePath() / ".crun/config.yaml");
    for (auto& path : {localYml, localYaml, globalYml, globalYaml}) {
        if (fs::exists(path) && !readConfig(path.string(), script)) {
            logMessage(INFO, "Найден конфиг: " + path.string());
            break;
        }
    }

    if (!arguments.scriptToRun.empty()) { return runScript(arguments.scriptToRun); }

    parseArgs(argc - 1, argv + 1);

    run();

    return 0;
}
