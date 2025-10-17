#pragma once
#define WIN32_LEAN_AND_MEAN

#include <string>

enum LogLevel { INFO, WARN, FAULT };

void setupConsoleUTF8();

void logMessage(const LogLevel& level, const std::string& msg, bool always = false);

void logMessage(const LogLevel& level, const std::string& msg, bool always, const std::string& prefix);

void logMessageA(const LogLevel& level, const std::string& msg, bool always = false);