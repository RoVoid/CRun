#include "../config.hpp"

#include <fstream>

#include "../args.hpp"
#include "../fkYAML/node.hpp"
#include "../logger.hpp"

extern Args arguments;

bool readConfig(const std::string& path, const std::string& script) {
    std::ifstream f(path);
    logMessage(INFO, path);
    if (!f.is_open()) return true;

    fkyaml::node doc;
    try {
        doc = fkyaml::node::deserialize(f);
    }
    catch (const std::exception& e) {
        logMessage(FAULT, std::string("Ошибка чтения конфига: ") + e.what());
        return true;
    }

    // --- обработка секции scripts ---
    if (!script.empty()) {
        auto scripts = doc["scripts"];
        if (scripts.is_mapping()) {
            auto scr = scripts[script];
            if (scr.empty()) {
                logMessage(FAULT, "Скрипт '" + script + "' не найден!");
                return true;
            }
            else {
                if (scr.is_string()) arguments.scriptToRun = scr.get_value<string>();
                else {
                    logMessage(FAULT, "Скрипт '" + script + "' имеет неверный тип, нужен string");
                    return true;
                }
            }
        }
    }

    // --- универсальные функции извлечения ---
    auto extractArray = [&](const char* key, std::set<std::string>& value) {
        auto n = doc[key];
        if (n.is_sequence()) {
            value.clear();
            for (auto& v : n.sequence())
                if (v.is_string()) value.insert(v.get_value<string>());
        }
    };
    auto extractString = [&](const char* key, std::string& value) {
        auto n = doc[key];
        if (n.is_string()) {
            value = n.get_value<string>();
            return true;
        }
        return false;
    };
    auto extractBool = [&](const char* key, bool& value) {
        auto n = doc[key];
        if (n.is_boolean()) value = n.get_value<bool>();
    };

    // --- загрузка всех настроек ---
    extractArray("includes", arguments.includeDirs);
    extractArray("libs-folders", arguments.libDirs);
    extractArray("libs", arguments.libsList);
    extractArray("folders", arguments.folders);
    extractArray("files", arguments.files);

    extractString("name", arguments.name);
    extractString("options", arguments.compilerOptions);
    extractBool("clear", arguments.clear);
    extractBool("downToC", arguments.downToC);
    extractString("build", arguments.buildFolder);

    string launch;
    if (extractString("launch", launch)) {
        if (launch == "run") arguments.launch = RUN;
        else if (launch == "build") arguments.launch = BUILD;
        else {
            logMessage(FAULT, "Неверное значение для launch, нужно 'run' или 'build'");
            arguments.launch = BOTH;
        }
    }

    string logLevel;
    if (extractString("log-level", logLevel)) {
        if (logLevel == "info") arguments.logLevel = INFO;
        else if (logLevel == "warn") arguments.logLevel = WARN;
        else if (logLevel == "error") arguments.logLevel = FAULT;
        else {
            logMessage(FAULT, "Неверное значение для launch, нужно 'info', 'warn' или 'error'");
            arguments.logLevel = WARN;
        }
    }

    return false;
}
