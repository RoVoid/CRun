#include "../monitor.hpp"

#include <thread>

inline static unsigned Max(unsigned a, unsigned b) { return a > b ? a : b; }

#ifdef _WIN32
static void collectProcessStats(HANDLE hProcess, MonitoringResult& result) {
    PROCESS_MEMORY_COUNTERS pmc{};
    FILETIME ftCreation, ftExit, ftKernel, ftUser;

    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        unsigned memMB = static_cast<unsigned>(pmc.WorkingSetSize / 1024 / 1024);
        result.ramMax = Max(result.ramMax, memMB);
        result.ramAverage = (result.ramAverage + memMB) / 2;
    }

    if (GetProcessTimes(hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser)) {
        ULONGLONG kernelTime = ((ULONGLONG)ftKernel.dwHighDateTime << 32) | ftKernel.dwLowDateTime;
        ULONGLONG userTime = ((ULONGLONG)ftUser.dwHighDateTime << 32) | ftUser.dwLowDateTime;
        unsigned cpu = static_cast<unsigned>((kernelTime + userTime) / 10000000ULL);
        result.cpuMax = Max(result.cpuMax, cpu);
        result.cpuAverage = (result.cpuAverage + cpu) / 2;
    }
}
#else
static void collectProcessStats(pid_t pid, MonitoringResult& result) {
    (void)pid;
    // вставить чтение /proc/self/stat для CPU/RAM
}
#endif

#ifdef _WIN32
void monitorProcess(DWORD pid, MonitoringResult& result) {
    std::thread([pid, &result]() {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!hProcess) return;

        while (true) {
            collectProcessStats(hProcess, result);

            DWORD code = 0;
            if (!GetExitCodeProcess(hProcess, &code) || code != STILL_ACTIVE) break;

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        CloseHandle(hProcess);
    }).detach();
}
#else
void monitorProcess(pid_t pid, MonitoringResult& result) {
    result = {};
    return;
    std::thread([pid, &result]() {
        MonitoringResult result{};
        while (true) {
            collectProcessStats(pid, result);

            // проверка завершения процесса на Linux (через kill(pid, 0))
            if (kill(pid, 0) != 0) break;

            if (callback) callback(result);

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }).detach();
}
#endif
