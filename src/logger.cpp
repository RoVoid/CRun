#include "../logger.hpp"

#include <iostream>

#include "../args.hpp"

using std::cout;
using std::string;

#ifdef _WIN32
#include <windows.h>

void setupConsoleUTF8() {
    // Устанавливаем кодировку консоли в UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Включаем ANSI escape-последовательности (для цветов)
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(hOut, &mode)) {
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, mode);
    }

    // Разрешаем вывод Unicode через cout
    std::ios::sync_with_stdio(false);
    std::locale::global(std::locale(""));  // устанавливаем системную локаль
}
#else
void setupConsoleUTF8() {
    std::ios::sync_with_stdio(false);
    std::locale::global(std::locale(""));
}
#endif

// private
static string getColor(const LogLevel& level) {
    switch (level) {
    case INFO:
        return "\033[36m";  // голубой
    case WARN:
        return "\033[33m";  // жёлтый
    case FAULT:
        return "\033[31m";  // красный
    default:
        return "\033[0m";  // сброс цвета
    }
}

// private
static string getPrefix(const LogLevel& level) {
    switch (level) {
    case INFO:
        return "ℹ️";
    case WARN:
        return "⚠️";
    case FAULT:
        return "❌";
    default:
        return "";
    }
}

void logMessage(const LogLevel& level, const string& msg, bool always, const string& prefix) {
    if (!always && level < arguments.logLevel) return;

    cout << getColor(level);
    if (!prefix.empty()) cout << prefix << " ";
    cout << msg << "\033[0m" << std::endl;
}

void logMessage(const LogLevel& level, const string& msg, bool always) {
    logMessage(level, msg, always, getPrefix(level));
}

void logMessageA(const LogLevel& level, const string& msg, bool always) { logMessage(level, msg, always, ""); }