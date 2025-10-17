#include "../config.hpp"

#include <fstream>

#include "../args.hpp"
#include "../logger.hpp"
#include "../rapidjson/document.h"

extern Args arguments;

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

bool readConfig(const std::string& path, const std::string& script) {
    std::string json = readFile(path);
    if (json.empty()) return true;

    rapidjson::Document doc;
    doc.Parse(json.c_str());
    if (doc.HasParseError()) {
        logMessage(FAULT, "Ошибка чтения конфига!");
        return true;
    }

    if (!script.empty()) {
        if (doc.HasMember("scripts") && doc["scripts"].IsObject()) {
            const auto& scripts = doc["scripts"].GetObject();
            auto it = scripts.FindMember(script.c_str());
            if (it != scripts.MemberEnd() && it->value.IsString()) { arguments.scriptToRun = it->value.GetString(); }
            else if (it != scripts.MemberEnd()) {
                logMessage(FAULT, "Скрипт '" + script + "' имеет неверный тип, нужен string");
                return true;
            }
            else {
                logMessage(FAULT, "Скрипт '" + script + "' не найден!");
                return true;
            }
        }
    }

    auto extractArray = [&](const char* key, std::vector<std::string>& value) {
        if (doc.HasMember(key) && doc[key].IsArray()) {
            value.clear();
            for (auto& v : doc[key].GetArray())
                if (v.IsString()) value.push_back(v.GetString());
        }
    };
    auto extractString = [&](const char* key, std::string& value) {
        if (doc.HasMember(key) && doc[key].IsString()) value = doc[key].GetString();
    };
    auto extractBool = [&](const char* key, bool& value) {
        if (doc.HasMember(key) && doc[key].IsBool()) value = doc[key].GetBool();
    };

    extractArray("includes", arguments.includeDirs);
    extractArray("libs-folders", arguments.libDirs);
    extractArray("libs", arguments.libsList);
    extractArray("folders", arguments.folders);
    extractArray("files", arguments.files);

    extractString("name", arguments.name);
    extractString("options", arguments.compilerOptions);
    extractBool("clear", arguments.clear);
    extractBool("useGCC", arguments.useGCC);

    extractString("build", arguments.buildFolder);
    return false;
}
