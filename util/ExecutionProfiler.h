#ifndef EXECUTIONPROFILER_H
#define EXECUTIONPROFILER_H
#include <unordered_map>
#include "Timer.h"
#include <string>
#pragma once

#ifdef EXEC_PROFILE
extern std::unordered_map<std::string, Timepoint> startTimes[NUMTHREADS];
extern std::unordered_map<std::string, size_t> durations[NUMTHREADS];
extern std::unordered_map<std::string, size_t> counters[NUMTHREADS];
#endif

struct ExecutionProfiler {

    static void startProfile(std::string name, int tid) {
#ifdef EXEC_PROFILE
        Timepoint &st = startTimes[tid][name];
        st = Now;
#endif
    }

    static void endProfile(std::string name, int tid) {
#ifdef EXEC_PROFILE
        auto end = Now;
        auto start = startTimes[tid][name];
        if (durations[tid].find(name) == durations[tid].end()) {
            durations[tid][name] = DurationNS(end - start);
            counters[tid][name] = 1;
        } else {
            durations[tid][name] += DurationNS(end - start);
            counters[tid][name]++;
        }
#endif
    }

    static void printProfile(std::ofstream& header, std::ofstream& fout) {
#ifdef EXEC_PROFILE
        for (int i = 0; i < NUMTHREADS; ++i)
            for (auto it : durations[i]) {
                header << ",T" << i << ":" << it.first << "-count,T" << i << ":" << it.first << "-time";
                fout << "," << counters[i][it.first] << "," << it.second / 100000.0;
            }

#endif
    }
};

#endif /* EXECUTIONPROFILER_H */

