#define NOMINMAX
#include <windows.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "rapidjson/document.h"

using namespace std;
namespace fs = filesystem;

static fs::path getExecutablePath() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (len == 0) throw runtime_error("GetModuleFileNameW failed");
    return fs::path(buffer).parent_path();
#else
    char buffer[4096];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len == -1) throw runtime_error("readlink(/proc/self/exe) failed");
    buffer[len] = '\0';
    return fs::path(buffer).parent_path();
#endif
}

enum Launch { BUILD, RUN, BOTH };

enum LogLevel { INFO, WARNING, FAULT, DEBUG };

struct Args {
    bool clear = false;
    Launch launch = BOTH;
    string buildFolder = "build";
    bool useGCC = false;
    string name;
    vector<string> files, folders, includeDirs, libDirs, libsList;
    string compilerOptions, exeArgs;
    LogLevel logLevel = FAULT;
};

Args arguments;

static void logMessage(LogLevel level, const string& msg, const string& customEmoji = "", bool always = false) {
    if (!always && level < arguments.logLevel) return;

    string color, emoji;
    switch (level) {
    case INFO:
        color = "\033[36m";
        emoji = "ℹ️";
        break;  // синий
    case WARNING:
        color = "\033[33m";
        emoji = "⚠️";
        break;  // желтый
    case FAULT:
        color = "\033[31m";
        emoji = "❌";
        break;  // красный
    case DEBUG:
        color = "\033[35m";
        emoji = "🐞";
        break;  // фиолетовый
    }

    if (!customEmoji.empty()) emoji = customEmoji;

    (level == FAULT ? cerr : cout) << color << emoji << ' ' << msg << "\033[0m" << endl;
}

// ------------------ Чтение конфигурации ------------------
static int scriptCallDepth = 0;
static const int MAX_SCRIPT_DEPTH = 5;

static bool runScript(const string& script) {
    if (++scriptCallDepth > MAX_SCRIPT_DEPTH) return true;
#ifdef _WIN32
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    vector<char> cmd(script.begin(), script.end());
    cmd.push_back(0);
    if (CreateProcessA(NULL, cmd.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
#else
    system(script.c_str());
#endif
    return false;
}

static string readFile(const string& path) {
    ifstream f(path);
    if (!f.is_open()) return {};
    return string((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
}

inline static bool extractString(const rapidjson::Document& d, const char* key, string& value) {
    if (d.HasMember(key)) {
        if (d[key].IsString()) {
            value = d[key].GetString();
            return true;
        }
        logMessage(FAULT, string("Неверный тип для ") + key + ", нужен string");
    }
    return false;
}

inline static bool extractBool(const rapidjson::Document& d, const char* key, bool& value) {
    if (d.HasMember(key)) {
        if (d[key].IsBool()) {
            value = d[key].GetBool();
            return true;
        }
        logMessage(FAULT, string("Неверный тип для ") + key + ", нужен bool");
    }
    return false;
}

inline static bool extractArray(const rapidjson::Document& d, const char* key, vector<string>& value) {
    if (d.HasMember(key)) {
        if (d[key].IsArray()) {
            value.clear();
            for (auto& v : d[key].GetArray()) {
                if (v.IsString())
                    value.push_back(v.GetString());
                else
                    logMessage(FAULT, string("Элемент массива ") + key + " не string");
            }
            return true;
        }
        logMessage(FAULT, string("Неверный тип для ") + key + ", нужен массив[string]");
    }
    return false;
}

bool readConfig(const string& path, const string& script) {
    string json = readFile(path);
    if (json.empty()) return true;

    rapidjson::Document doc;
    doc.Parse(json.c_str());
    if (doc.HasParseError()) {
        logMessage(FAULT, "Ошибка чтения конфига!");
        return true;
    }

    if (!script.empty() && scriptCallDepth < MAX_SCRIPT_DEPTH) {
        regex rgx(R"("scripts"\s*:\s*\{([^}]*)\})");
        smatch match;
        if (regex_search(json, match, rgx)) {
            string scriptsBlock = match[1];
            regex pairRgx("\"([^\"]+)\"\\s*:\\s*\"([^\"]+)\"");
            for (auto it = sregex_iterator(scriptsBlock.begin(), scriptsBlock.end(), pairRgx); it != sregex_iterator(); ++it) {
                if ((*it)[1] == script) return runScript((*it)[2]);
            }
        }
    }

    extractArray(doc, "includes", arguments.includeDirs);
    extractArray(doc, "libs-folders", arguments.libDirs);
    extractArray(doc, "libs", arguments.libsList);
    extractArray(doc, "folders", arguments.folders);
    extractArray(doc, "files", arguments.files);

    extractString(doc, "name", arguments.name);
    extractString(doc, "options", arguments.compilerOptions);
    extractBool(doc, "clear", arguments.clear);
    extractBool(doc, "useGCC", arguments.useGCC);

    if (extractString(doc, "build", arguments.buildFolder)) {
        fs::path buildPath(arguments.buildFolder);
        if (!fs::is_directory(buildPath) && fs::exists(buildPath)) {
            logMessage(FAULT, "Путь для сборки должна быть папка: " + arguments.buildFolder);
            arguments.buildFolder = "build";
        }
    }

    string launch;
    if (extractString(doc, "launch", launch)) {
        if (launch == "run")
            arguments.launch = RUN;
        else if (launch == "build")
            arguments.launch = BUILD;
        else {
            logMessage(FAULT, "Неверное значение для launch, нужно 'run' или 'build'");
            arguments.launch = BOTH;
        }
    }

    string logLevel;
    if (extractString(doc, "log-level", logLevel)) {
        if (logLevel == "info")
            arguments.logLevel = INFO;
        else if (logLevel == "warn")
            arguments.logLevel = WARNING;
        else if (logLevel == "error")
            arguments.logLevel = FAULT;
        else if (logLevel == "debug")
            arguments.logLevel = DEBUG;
        else {
            logMessage(FAULT, "Неверное значение для launch, нужно 'info', 'warn', 'error' или 'debug'");
            arguments.logLevel = WARNING;
        }
    }

    return false;
}

// ------------------ Аргументы ------------------

void parseArgs(int argc, char* argv[]) {
    static auto readUntilBackslash = [&](int& i, string& s) {
        s.clear();
        while (++i < argc && argv[i][0] != '\\') {
            if (!s.empty()) s += " ";
            s += argv[i];
        }
    };

    static auto pushNextArg = [&](int& i, vector<string>& v) {
        if (i + 1 < argc) v.push_back(argv[++i]);
    };

    static auto setNextArg = [&](int& i, string& s) {
        if (i + 1 < argc) s = argv[++i];
    };

    for (int i = 0; i < argc; ++i) {
        const string& arg = argv[i];

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
            arguments.logLevel = WARNING;
        else if (arg == "-error")
            arguments.logLevel = FAULT;
        else if (arg == "-debug")
            arguments.logLevel = DEBUG;
        else {
            // bug: clear стирает все эти сообщения
            // bug: чтобы увидеть сообщения ДО ЭТОГО должен быть -warn | -info
            if (arg.empty())
                logMessage(WARNING, "Имя файла не может быть пустым!");
            else if (arg[0] == '-')
                logMessage(WARNING, "Неверный аргумент: " + arg);
            else {
                fs::path p(arg);
                if (!fs::exists(p))
                    logMessage(WARNING, "Файл не найден: " + arg);
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

// ------------------ Компиляция и запуск ------------------
void run() {
    if (arguments.clear) {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
    }

    logMessage(INFO, "Папка сборки: " + arguments.buildFolder, "📂");

    fs::create_directories(arguments.buildFolder);
    fs::path outputPath = fs::absolute(fs::path(arguments.buildFolder) / (arguments.name + ".exe"));
    string compiler = arguments.useGCC ? "gcc" : "g++";

    static const unordered_set<string> exts = {".cpp", ".c"};

    for (auto& folder : arguments.folders) {
        if (!fs::exists(folder)) continue;
        for (auto& p : fs::directory_iterator(folder))
            if (p.is_regular_file() && exts.count(p.path().extension().string())) arguments.files.push_back(p.path().string());
    }

    auto joinQuoted = [](const vector<string>& v, const string& pre = "", const string& post = "") -> string {
        ostringstream oss;
        for (auto& x : v) oss << ' ' << pre << '"' << x << '"' << post;
        return oss.str();
    };

    string filesStr = joinQuoted(arguments.files);
    string includeStr = joinQuoted(arguments.includeDirs, "-I");
    string libDirStr = joinQuoted(arguments.libDirs, "-L");
    string libsStr;
    for (auto& lib : arguments.libsList) libsStr += " -l" + lib;

    if (arguments.launch != RUN) {
        ostringstream ss;
        ss << compiler << filesStr << libDirStr << includeStr << libsStr << " " << arguments.compilerOptions << " -o \"" << outputPath.string() << "\" -finput-charset=UTF-8";
        logMessage(INFO, "Начало " + compiler + " сборки " + arguments.name, "⚒️", true);
        if (system(ss.str().c_str()) != 0) {
            logMessage(FAULT, "Ошибка при компиляции!", "❌", true);
            return;
        }
        logMessage(INFO, "Сборка завершена", "✅", true);
    }

    if (arguments.launch != BUILD) {
        if (!fs::exists(outputPath)) {
            logMessage(FAULT, "Исполняемый файл не найден!", "❓");
            return;
        }
        string cmd = "\"" + outputPath.string() + "\" " + arguments.exeArgs;
        logMessage(INFO, "Запуск программы", "➡️", true);
        int ret;
        if ((ret = system(cmd.c_str())) != 0) {
            logMessage(FAULT, "Завершена с ошибкой (код: " + to_string(ret) + ")", "❌", true);
            return;
        }
        logMessage(INFO, "Успешное завершение", "⏹️", true);
    }
}

// ------------------ MAIN ------------------
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
            logMessage(WARNING, "Пока это не работает");
            return 0;
        }
        else if (command == "v" || command == "version") {
            logMessage(INFO, "CRUN 0.2 by RoVoid");
            return 0;
        }
        else if (command == "h" || command == "help") {
            logMessage(INFO, "CRUN — компилятор и запуск C/C++ проектов", "🛠️", true);

            logMessage(INFO, "Команды:", "📌", true);
            logMessage(INFO, "  run <script>      — выполнить скрипт из crun.json", " ", true);
            logMessage(INFO, "  init              — создать шаблон crun.json", " ", true);
            logMessage(INFO, "  version           — показать версию", " ", true);
            logMessage(INFO, "  help              — показать эту справку", " ", true);

            logMessage(INFO, "Флаги:", "🏷️", true);
            logMessage(INFO, "  -c, -clear        — очистить консоль перед запуском", " ", true);
            logMessage(INFO, "  -r, -run          — запуск после сборки", " ", true);
            logMessage(INFO, "  -b, -build        — только сборка", " ", true);
            logMessage(INFO, "  -gcc              — использовать gcc вместо g++", " ", true);
            logMessage(INFO, "  -g++              — использовать g++", " ", true);
            logMessage(INFO, "  -bd, -buildDir    — указать папку сборки", " ", true);
            logMessage(INFO, "  -I <dir>          — добавить include папку", " ", true);
            logMessage(INFO, "  -L <dir>          — добавить папку с библиотеками", " ", true);
            logMessage(INFO, "  -l <lib>          — добавить библиотеку", " ", true);
            logMessage(INFO, "  -F <folder>       — добавить папку с исходниками", " ", true);
            logMessage(INFO, "  -f <file>         — добавить файл", " ", true);
            logMessage(INFO, "  -o <options...>   — дополнительные опции компилятора", " ", true);
            logMessage(INFO, "  -- <...>          — аргументы для исполняемого файла", " ", true);

            return 0;
        }
    }

    fs::path localPath = fs::absolute("./crun.json");
    fs::path globalPath = fs::absolute(getExecutablePath() / "crun.json");

    if (readConfig(localPath.string(), script) && !fs::equivalent(localPath, globalPath)) {
        readConfig(globalPath.string(), script);
    }

    parseArgs(argc - 1, argv + 1);
    run();
    return 0;
}
