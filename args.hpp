#pragma once
#include <string>
#include <vector>

#include "logger.hpp"

enum Launch { BUILD, RUN, BOTH };
enum LogLevel;

struct Args {
    bool clear = false;
    Launch launch = BOTH;
    std::string buildFolder = "build";
    bool useGCC = false;
    std::string name;
    std::vector<std::string> files, folders, includeDirs, libDirs, libsList;
    std::string compilerOptions, exeArgs;
    LogLevel logLevel = FAULT;
    std::string scriptToRun = "";
};

extern Args arguments;

void parseArgs(int argc, char* argv[]);
