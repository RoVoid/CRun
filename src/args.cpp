#include "../args.hpp"

#include <filesystem>
#include <string>
#include <vector>

#include "../logger.hpp"

namespace fs = std::filesystem;
Args arguments;

void parseArgs(int argc, char* argv[]) {
    auto readUntilBackslash = [&](int& i, std::string& s) {
        s.clear();
        while (++i < argc && argv[i][0] != '\\') {
            if (!s.empty()) s += " ";
            s += argv[i];
        }
    };
    auto pushNextArg = [&](int& i, std::vector<std::string>& v) {
        if (i + 1 < argc) v.push_back(argv[++i]);
    };
    auto setNextArg = [&](int& i, std::string& s) {
        if (i + 1 < argc) s = argv[++i];
    };

    for (int i = 0; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "-o")
            readUntilBackslash(i, arguments.compilerOptions);
        else if (arg == "--")
            readUntilBackslash(i, arguments.exeArgs);
        else if (arg == "-c" || arg == "-clear")
            arguments.clear = true;
        else if (arg == "-r" || arg == "-run")
            arguments.launch = RUN;
        else if (arg == "-b" || arg == "-build")
            arguments.launch = BUILD;
        else if (arg == "-gcc")
            arguments.useGCC = true;
        else if (arg == "-g++")
            arguments.useGCC = false;
        else if (arg == "-bd" || arg == "-buildDir")
            setNextArg(i, arguments.buildFolder);
        else if (arg == "-I")
            pushNextArg(i, arguments.includeDirs);
        else if (arg == "-L")
            pushNextArg(i, arguments.libDirs);
        else if (arg == "-l")
            pushNextArg(i, arguments.libsList);
        else if (arg == "-F")
            pushNextArg(i, arguments.folders);
        else if (arg == "-f")
            pushNextArg(i, arguments.files);
        else if (arg == "-info")
            arguments.logLevel = INFO;
        else if (arg == "-warn")
            arguments.logLevel = WARN;
        else if (arg == "-error")
            arguments.logLevel = FAULT;
        else {
            if (arg.empty())
                logMessage(WARN, "Имя файла не может быть пустым!");
            else if (arg[0] == '-')
                logMessage(WARN, "Неверный аргумент: " + arg);
            else {
                fs::path p(arg);
                if (!fs::exists(p))
                    logMessage(WARN, "Файл не найден: " + arg);
                else
                    arguments.files.push_back(arg);
            }
        }
    }

    if (arguments.name.empty()) {
        if (arguments.files.empty()) {
            arguments.files.push_back(arguments.useGCC ? "main.c" : "main.cpp");
            arguments.name = "main";
        }
        if (arguments.name.empty() && !arguments.files.empty()) {
            fs::path p(arguments.files[0]);
            arguments.name = p.stem().string();
        }
    }
}
