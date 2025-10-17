#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* clang-off */
#include <psapi.h>

using ProcessId = DWORD;
#else
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>

using ProcessId = pid_t;
#endif

struct MonitoringResult {
    unsigned cpuAverage = 0;  // среднее использование CPU
    unsigned cpuMax = 0;      // максимум CPU
    unsigned ramAverage = 0;  // среднее использование RAM
    unsigned ramMax = 0;      // максимум RAM
};

void collectProcessStats(ProcessId pid, MonitoringResult& result);
void shutdownMonitor();
void monitorProcess(ProcessId pid, MonitoringResult& result);
