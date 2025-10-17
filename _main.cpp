#define NOMINMAX
#include <windows.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "rapidjson/document.h"

using namespace std;
namespace fs = filesystem;

#define VERSION "0.2 Alpha"

// fix: Код не кроссплатформенный

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

static void logMessage(LogLevel level, const string& msg, bool always = false, const string& emoji = "") {
    if (!always && level < arguments.logLevel) return;

    string color;
    switch (level) {
    case INFO:
        color = "\033[36m";
        break;  // голубой
    case WARNING:
        color = "\033[33m";
        break;  // жёлтый
    case FAULT:
        color = "\033[31m";
        break;  // красный
    case DEBUG:
        color = "\033[35m";
        break;  // фиолетовый
    }

    auto& out = (level == FAULT ? cerr : cout);

    out << color;
    if (!emoji.empty()) out << emoji << ' ';
    out << msg << "\033[0m" << endl;
}

static void logWithEmoji(LogLevel level, const string& msg, bool always = false, const string& customEmoji = "") {
    string emoji;
    if (!customEmoji.empty())
        emoji = customEmoji;
    else {
        switch (level) {
        case INFO:
            emoji = "ℹ️";
            break;
        case WARNING:
            emoji = "⚠️";
            break;
        case FAULT:
            emoji = "❌";
            break;
        case DEBUG:
            emoji = "🐞";
            break;
        }
    }
    logMessage(level, msg, always, emoji);
}

static bool runScript(const string&);

// ------------------ Чтение конфигурации ------------------
static int scriptCallDepth = 0;
static const int MAX_SCRIPT_DEPTH = 5;

static string readFile(const string& path) {
    ifstream f;
    try {
        f.open(path);
        if (!f.is_open()) return {};
    }
    catch (...) {
        return {};
    }
    return string((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
}

inline static bool extractString(const rapidjson::Document& d, const char* key, string& value) {
    if (d.HasMember(key)) {
        if (d[key].IsString()) {
            value = d[key].GetString();
            return true;
        }
        logWithEmoji(FAULT, string("Неверный тип для ") + key + ", нужен string");
    }
    return false;
}

inline static bool extractBool(const rapidjson::Document& d, const char* key, bool& value) {
    if (d.HasMember(key)) {
        if (d[key].IsBool()) {
            value = d[key].GetBool();
            return true;
        }
        logWithEmoji(FAULT, string("Неверный тип для ") + key + ", нужен bool");
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
                    logWithEmoji(FAULT, string("Элемент массива ") + key + " не string");
            }
            return true;
        }
        logWithEmoji(FAULT, string("Неверный тип для ") + key + ", нужен массив[string]");
    }
    return false;
}

bool readConfig(const string& path, const string& script) {
    string json = readFile(path);
    if (json.empty()) return true;

    rapidjson::Document doc;
    doc.Parse(json.c_str());
    if (doc.HasParseError()) {
        logWithEmoji(FAULT, "Ошибка чтения конфига!");
        return true;
    }

    if (!script.empty() && scriptCallDepth < MAX_SCRIPT_DEPTH) {
        if (doc.HasMember("scripts") && doc["scripts"].IsObject()) {
            const auto& scripts = doc["scripts"].GetObject();
            auto it = scripts.FindMember(script.c_str());
            if (it != scripts.MemberEnd() && it->value.IsString()) { exit(runScript(it->value.GetString())); }
            else if (it != scripts.MemberEnd()) {
                logWithEmoji(FAULT, "Скрипт '" + script + "' имеет неверный тип, нужен string");
                exit(2);
            }
        }
        logWithEmoji(FAULT, "Скрипт '" + script + "' не найден!");
        exit(3);
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
            logWithEmoji(FAULT, "Путь для сборки должна быть папка: " + arguments.buildFolder);
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
            logWithEmoji(FAULT, "Неверное значение для launch, нужно 'run' или 'build'");
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
            logWithEmoji(FAULT, "Неверное значение для launch, нужно 'info', 'warn', 'error' или 'debug'");
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

        // fix: Нельзя задать name
        // fix: Нет возможности отрицать
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
            // bug: чтобы увидеть сообщения ДО ЭТОГО должен быть -warn | -info
            if (arg.empty())
                logWithEmoji(WARNING, "Имя файла не может быть пустым!");
            else if (arg[0] == '-')
                logWithEmoji(WARNING, "Неверный аргумент: " + arg);
            else {
                fs::path p(arg);
                if (!fs::exists(p))
                    logWithEmoji(WARNING, "Файл не найден: " + arg);
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

// note: Нужно больше информации о выполнении
// fix: Следует изменить сообщения
void run() {
    logWithEmoji(INFO, "Папка сборки: " + arguments.buildFolder, false, "📂");
    if (!arguments.files.empty()) {
        logWithEmoji(INFO, "Файлы сборки: ", false, "📚");
        for (auto file : arguments.files) { logMessage(INFO, "   * " + file); }
    }

    fs::create_directories(arguments.buildFolder);
    fs::path outputPath = fs::absolute(fs::path(arguments.buildFolder) / (arguments.name + ".exe"));
    string compiler = arguments.useGCC ? "gcc" : "g++";

    static const unordered_set<string> exts = {".cpp", ".c"};

    for (auto& folder : arguments.folders) {
        if (!fs::exists(folder)) continue;
        for (auto& p : fs::directory_iterator(folder))
            if (p.is_regular_file() && exts.count(p.path().extension().string()))
                arguments.files.push_back(p.path().string());
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
        ss << compiler << filesStr << libDirStr << includeStr << libsStr << " " << arguments.compilerOptions << " -o \""
           << outputPath.string() << "\" -finput-charset=UTF-8";
        logWithEmoji(INFO, "Начало " + compiler + " сборки " + arguments.name, true, "⚒️");
        if (system(ss.str().c_str()) != 0) {
            logWithEmoji(FAULT, "Ошибка при компиляции!", true);
            return;
        }
        logWithEmoji(INFO, "Сборка завершена", true, "✅");
    }

    if (arguments.launch != BUILD) {
        if (!fs::exists(outputPath)) {
            logWithEmoji(FAULT, "Исполняемый файл не найден!", "❓");
            return;
        }

        string cmd = "\"" + outputPath.string() + "\" " + arguments.exeArgs;
        runAndMonitor(cmd);

        // logWithEmoji(INFO, "Запуск программы", true, "➡️");
        // int ret;
        // if ((ret = system(cmd.c_str())) != 0) {
        //     logWithEmoji(FAULT, "Завершена с ошибкой (код: " + to_string(ret) + ")", true);
        //     return;
        // }
        // logWithEmoji(INFO, "Успешное завершение", true, "⏹️");
    }
}

// ------------------ MAIN ------------------
void enableANSIColors() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode)) return;
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, mode);
}

int main(int argc, char* argv[]) {
    enableANSIColors();
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    string script;
    if (argc >= 2) {
        string command = argv[1];
        if (command == "r" || command == "run") {
            if (argc == 2) {
                logWithEmoji(FAULT, "Нужно имя скрипта");
                return 1;
            }
            script = argv[2];
        }
        else if (command == "i" || command == "init") {
            logWithEmoji(WARNING, "Пока это не работает");
            return 0;
        }
        else if (command == "v" || command == "version") {
            logWithEmoji(INFO, string("CRUN ") + VERSION, true);
            return 0;
        }
        else if (command == "h" || command == "help") {
            logWithEmoji(INFO, "CRUN — компилятор и запуск C/C++ проектов", true, "🛠️");

            logWithEmoji(INFO, "Команды:", true, "📌");
            logMessage(INFO, "    run <script>      — выполнить скрипт из crun.json", true);
            logMessage(INFO, "    init              — создать шаблон crun.json", true);
            logMessage(INFO, "    version           — показать версию", true);
            logMessage(INFO, "    help              — показать эту справку", true);

            logWithEmoji(INFO, "Флаги:", true, "🏷️");
            logMessage(INFO, "    -c, -clear        — очистить консоль перед запуском", true);
            logMessage(INFO, "    -r, -run          — запуск после сборки", true);
            logMessage(INFO, "    -b, -build        — только сборка", true);
            logMessage(INFO, "    -gcc              — использовать gcc вместо g++", true);
            logMessage(INFO, "    -g++              — использовать g++", true);
            logMessage(INFO, "    -bd, -buildDir    — указать папку сборки", true);
            logMessage(INFO, "    -I <dir>          — добавить include папку", true);
            logMessage(INFO, "    -L <dir>          — добавить папку с библиотеками", true);
            logMessage(INFO, "    -l <lib>          — добавить библиотеку", true);
            logMessage(INFO, "    -F <folder>       — добавить папку с исходниками", true);
            logMessage(INFO, "    -f <file>         — добавить файл", true);
            logMessage(INFO, "    -o <options...>   — дополнительные опции компилятора", true);
            logMessage(INFO, "    -- <...>          — аргументы для исполняемого файла", true);

            return 0;
        }
    }

    fs::path localPath = fs::absolute("./crun.json");
    if (readConfig(localPath.string(), script)) {
        try {
            fs::path globalPath = fs::absolute(getExecutablePath() / "crun.json");
            if (!fs::equivalent(localPath, globalPath)) readConfig(globalPath.string(), script);
        }
        catch (...) {
        }
    }

    parseArgs(argc - 1, argv + 1);

    if (arguments.clear) {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
    }

    run();
    return 0;
}
