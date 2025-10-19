#pragma once
#include <set>
#include <string>

#include "logger.hpp"

enum Launch { BUILD, RUN, BOTH };
enum LogLevel;

using std::string;

struct Args {
    bool clear = false;
    Launch launch = BOTH;
    string buildFolder = "build";
    bool downToC = false;
    string name;
    std::set<string> files, folders, includeDirs, libDirs, libsList;
    string compiler, compilerOptions, exeArgs;
    LogLevel logLevel = FAULT;
    string scriptToRun = "";
};

extern Args arguments;

void parseArgs(int argc, char* argv[]);
