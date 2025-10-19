#include "../args.hpp"

#include <filesystem>

#include "../logger.hpp"

namespace fs = std::filesystem;
Args arguments;

void parseArgs(int argc, char* argv[]) {
    auto readUntilBackslash = [&](int& i, string& s) {
        s.clear();
        while (++i < argc && argv[i][0] != '\\') {
            if (!s.empty()) s += " ";
            s += argv[i];
        }
    };
    auto pushNextArg = [&](int& i, std::set<string>& v) {
        if (i + 1 < argc) v.insert(argv[++i]);
    };
    auto setNextArg = [&](int& i, string& s) {
        if (i + 1 < argc) s = argv[++i];
    };

    for (int i = 0; i < argc; ++i) {
        const string arg = argv[i];

        if (arg == "-o") readUntilBackslash(i, arguments.compilerOptions);
        else if (arg == "--") readUntilBackslash(i, arguments.exeArgs);
        else if (arg == "-c" || arg == "-clear") arguments.clear = true;
        else if (arg == "-r" || arg == "-run") arguments.launch = RUN;
        else if (arg == "-b" || arg == "-build") arguments.launch = BUILD;
        else if (arg == "-gcc") arguments.downToC = true;
        else if (arg == "-g++") arguments.downToC = false;
        else if (arg == "-n" || arg == "-name") setNextArg(i, arguments.name);
        else if (arg == "-bd" || arg == "-buildDir") setNextArg(i, arguments.buildFolder);
        else if (arg == "-i" || arg == "-include") pushNextArg(i, arguments.includeDirs);
        else if (arg == "-l" || arg == "-lib") {
            if (i + 1 < argc) break;
            string next = argv[i + 1];
            if (next[0] == '-') logMessage(WARN, "Неверный аргумент: " + next);
            else {
                fs::path p(next);
                if (!fs::exists(p)) logMessage(WARN, "Не найден: " + next);
                else if (fs::is_directory(p)) arguments.libDirs.insert(next);
                else arguments.libsList.insert(next);
            }
        }
        else if (arg == "-info") arguments.logLevel = INFO;
        else if (arg == "-warn") arguments.logLevel = WARN;
        else if (arg == "-error") arguments.logLevel = FAULT;
        else {
            if (arg.empty()) logMessage(WARN, "Имя файла не может быть пустым!");
            else if (arg[0] == '-') logMessage(WARN, "Неверный аргумент: " + arg);
            else {
                fs::path p(arg);
                if (!fs::exists(p)) logMessage(WARN, "Не найден: " + arg);
                else if (fs::is_directory(p)) arguments.folders.insert(arg);
                else arguments.files.insert(arg);
            }
        }
    }
}
