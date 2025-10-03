#define NOMINMAX

#include <windows.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "rapidjson/document.h"


namespace fs = std::filesystem;
using namespace std;

enum Launch { BUILD, RUN, BOTH };

struct Args {
    bool clear = false;
    Launch launch = BOTH;
    string buildFolder = "./build";
    bool useGCC = false;
    string name;
    vector<string> files;
    vector<string> folders;
    vector<string> includeDirs;
    vector<string> libDirs;
    vector<string> libsList;
    string compilerOptions;
    string exeArgs;
    bool warnings = true;
};

Args arguments;

void help() { cout << "CRUN help\n"; }
void setBuildFolder(const string& dir) {
    if (!dir.empty()) arguments.buildFolder = dir;
}

map<string, function<void()>> flags = {{"-c", [] { arguments.clear = true; }},
    {"-clear", [] { arguments.clear = true; }},
    {"-r", [] { arguments.launch = RUN; }},
    {"-b", [] { arguments.launch = BUILD; }},
    {"-run", [] { arguments.launch = RUN; }},
    {"-build", [] { arguments.launch = BUILD; }},
    {"-gcc", [] { arguments.useGCC = true; }}};

map<string, function<void(const string&)>> options = {{"-bd", setBuildFolder}, {"-buildDir", setBuildFolder}};

void parseArgs(int argc, char* argv[]) {
    for (int i = 0; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--") {
            for (++i; i < argc; ++i) arguments.exeArgs += string(argv[i]) + " ";
            break;
        }
        if (auto it = flags.find(arg); it != flags.end())
            it->second();
        else if (auto it = options.find(arg); it != options.end()) {
            if (i + 1 < argc) it->second(argv[++i]);
        }
        else if (arg == "-I" && i + 1 < argc)
            arguments.includeDirs.push_back(argv[++i]);
        else if (arg == "-L" && i + 1 < argc)
            arguments.libDirs.push_back(argv[++i]);
        else if (arg == "-l" && i + 1 < argc)
            arguments.libsList.push_back(argv[++i]);
        else if (arg == "-F" && i + 1 < argc)
            arguments.folders.push_back(argv[++i]);
        else if (arg == "-f" && i + 1 < argc)
            arguments.files.push_back(argv[++i]);
        else if (arg == "-o") {
            for (++i; i < argc && argv[i][0] != '\\'; ++i) arguments.compilerOptions += string(argv[i]) + " ";
            --i;
        }
        else {
            fs::path p(arg);
            if (arguments.files.empty()) {
                arguments.name = p.stem().string();
                arguments.buildFolder = (p.parent_path() / arguments.buildFolder).string();
            }
            arguments.files.push_back(arg);
        }
    }
}

inline void readStringArray(const rapidjson::Document& d, const char* key, vector<string>& value) {
    if (d.HasMember(key) && d[key].IsArray())
        for (auto& v : d[key].GetArray())
            if (v.IsString()) value.push_back(v.GetString());
}

inline void readString(const rapidjson::Document& d, const char* key, string& value) {
    if (d.HasMember(key) && d[key].IsString()) value = d[key].GetString();
}

inline void readBool(const rapidjson::Document& d, const char* key, bool& value) {
    if (d.HasMember(key) && d[key].IsBool()) value = d[key].GetBool();
}

bool readConfig(const string& path, const string& script) {
    using namespace rapidjson;
    ifstream ifs(path);
    if (!ifs.is_open()) return true;
    stringstream buffer;
    buffer << ifs.rdbuf();
    Document d;
    d.Parse(buffer.str().c_str());
    if (d.HasParseError()) return true;

    static int scriptDepth = 0;
    if (!script.empty() && d.HasMember("scripts") && d["scripts"].IsObject() && scriptDepth < 2) {
        ++scriptDepth;
        for (auto& v : d["scripts"].GetObject()) {
            if (v.name.IsString() && v.name.GetString() == script && v.value.IsString()) {
                system(v.value.GetString());
                exit(0);
            }
        }
    }

    auto parseStringArray = [](const Value& arr, vector<string>& target) {
        if (arr.IsArray())
            for (auto& v : arr.GetArray())
                if (v.IsString()) target.push_back(v.GetString());
    };

    readStringArray(d, "includeDirs", arguments.includeDirs);
    readStringArray(d, "libDirs", arguments.libDirs);
    readStringArray(d, "libsList", arguments.libsList);
    readStringArray(d, "folders", arguments.folders);
    readStringArray(d, "files", arguments.files);

    readString(d, "name", arguments.name);
    readString(d, "compilerOptions", arguments.compilerOptions);
    readString(d, "buildFolder", arguments.buildFolder);

    readBool(d, "clear", arguments.clear);
    readBool(d, "useGCC", arguments.useGCC);

    string launch;
    readString(d, "launch", launch);
    if (launch == "run")
        arguments.launch = RUN;
    else if (launch == "build")
        arguments.launch = BUILD;

    return false;
}

void run() {
    if (arguments.clear) {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
    }
    fs::create_directories(arguments.buildFolder);
    string outputFile = arguments.buildFolder + "/" + arguments.name + ".exe";
    string compiler = arguments.useGCC ? "gcc" : "g++";

    for (auto& folder : arguments.folders)
        if (fs::exists(folder) && fs::is_directory(folder))
            for (auto& p : fs::directory_iterator(folder))
                if (p.is_regular_file()) {
                    auto ext = p.path().extension().string();
                    if (ext == ".cpp" || ext == ".c") arguments.files.push_back(p.path().string());
                }

    auto joinQuoted = [](const vector<string>& v, const string& prefix = "", const string& suffix = "") {
        string s;
        for (auto& x : v) s += " " + prefix + "\"" + x + "\"" + suffix;
        return s;
    };

    string filesStr = joinQuoted(arguments.files);
    string includeStr = joinQuoted(arguments.includeDirs, "-I");
    string libDirStr = joinQuoted(arguments.libDirs, "-L");
    string libsStr;
    for (auto& lib : arguments.libsList) libsStr += " -l" + lib;

    if (arguments.launch == RUN) {
        if (!fs::exists(outputFile)) {
            cerr << "❌ exe не найден: " << outputFile << endl;
            return;
        }
        string runCmd = "\"" + fs::absolute(outputFile).string() + "\" " + arguments.exeArgs;
        system(runCmd.c_str());
        return;
    }

    stringstream ss;
    ss << compiler << " " << filesStr << libDirStr << includeStr << libsStr << " " << arguments.compilerOptions << " -o \"" << outputFile << "\" -finput-charset=UTF-8";
    if (system(ss.str().c_str()) != 0) {
        cerr << "❌ Ошибка при компиляции\n";
        return;
    }

    if (arguments.launch != BUILD) {
        string runCmd = "\"" + fs::absolute(outputFile).string() + "\" " + arguments.exeArgs;
        system(runCmd.c_str());
    }
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    string script;
    if (argc >= 2) {
        string command = argv[1];
        if (command == "r" || command == "run") {
            if (argc == 2) {
                cerr << "Need script name\n";
                return 1;
            }
            script = argv[2];
        }
        else if (command == "i" || command == "init") {
            cout << "Init config\n";
            return 0;
        }
        else if (command == "v" || command == "version") {
            cout << "CRUN: Version 0.1 Alpha\n";
            return 0;
        }
        else if (command == "h" || command == "help") {
            help();
            return 0;
        }
    }
    if (readConfig("./crun.json", script)) {
        fs::path exeDir = fs::absolute(argv[0]).parent_path();
        readConfig((exeDir / "crun.json").string(), script);
    }
    parseArgs(argc - 1, argv + 1);
    run();
    return 0;
}
