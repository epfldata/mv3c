#pragma once

#include <sstream>
#include <iostream>
#include <functional>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

struct System {

    static void perfRecord(const std::string& name, std::function<void() > body) {
        std::string filename = name.find(".data") == std::string::npos ? (name + ".data") : name;
        const std::string hw_events = "branches,branch-misses,cache-misses,cache-references,cycles,instructions,stalled-cycles-backend,stalled-cycles-frontend";
        const std::string cache_events = "L1-dcache-load-misses,L1-dcache-loads,L1-icache-load-misses,LLC-load-misses,LLC-loads,dTLB-load-misses,dTLB-loads,dTLB-store-misses,dTLB-stores,iTLB-load-misses,iTLB-loads";
        const std::string syscall_events = "\'syscalls:sys_enter_*\'";
        const std::string sched_events = "\'sched:sched_*\'";
        // Launch profiler
        pid_t pid;
        cerr << "called perf record" << endl;
        std::stringstream s;
        s << getpid();
        pid = fork();
        if (pid == 0) {
            //            auto fd = open("/dev/null", O_RDWR);
            //            dup2(fd, 1);
            //            dup2(fd, 2);
            exit(execl("/usr/bin/perf", "sudo perf", "record", "-g", "-e", hw_events.c_str(), "-o", filename.c_str(), "-p", s.str().c_str(), nullptr));
        }

        // Run body
        body();

        // Kill profiler  
        kill(pid, SIGINT);
        waitpid(pid, nullptr, 0);
    }

    static void perfStat(const std::string& name, std::function<void() > body) {
        std::string filename = name + ".stat";
        const std::string hw_events = "branches,branch-misses,cache-misses,cache-references,cycles,instructions,stalled-cycles-backend,stalled-cycles-frontend";
        const std::string cache_events = "L1-dcache-load-misses,L1-dcache-loads,L1-icache-load-misses,LLC-load-misses,LLC-loads,dTLB-load-misses,dTLB-loads,dTLB-store-misses,dTLB-stores,iTLB-load-misses,iTLB-loads";
        const std::string syscall_events = "\'syscalls:sys_enter_*\'";
        const std::string sched_events = "\'sched:sched_*\'";
        // Launch profiler
        //        const std::string stat_events = syscall_events; // + "," + sched_events;
        const std::string stat_events = hw_events + "," + cache_events + "," + syscall_events + "," + sched_events;
        cerr << "called perf stat  -e  " << stat_events << endl;

        pid_t pid;
        std::stringstream s;
        s << getpid();
        pid = fork();
        if (pid == 0) {
            //            auto fd = ::open(filename.c_str(), O_WRONLY);
            //            ::dup2(fd, 1);
            //            dup2(fd, 2);
            exit(execl("/usr/bin/perf", "sudo perf", "stat", "-e", stat_events.c_str(), "-o", filename.c_str(), "-p", s.str().c_str(), nullptr));

        }

        // Run body
        body();

        // Kill profiler  
        kill(pid, SIGINT);
        waitpid(pid, nullptr, 0);
    }

    //    static void perf(const std::string& name, std::function<void() > body) {
    //        std::string filename_stat = name + ".stat";
    //        std::string filename_data = name + ".data";
    //        const std::string hw_events = "branches,branch-misses,cache-misses,cache-references,cycles,instructions,stalled-cycles-backend,stalled-cycles-frontend";
    //        const std::string cache_events = "L1-dcache-load-misses,L1-dcache-loads,L1-icache-load-misses,LLC-load-misses,LLC-loads,dTLB-load-misses,dTLB-loads,dTLB-store-misses,dTLB-stores,iTLB-load-misses,iTLB-loads";
    //        const std::string syscall_events = "\'syscalls:sys_enter_*\'";
    //        const std::string sched_events = "\'sched:sched_*\'";
    //        // Launch profiler
    //        cerr << "called perf" << endl;
    //        std::stringstream s;
    //        s << getpid();
    //
    //        pid_t pid_stat;
    //        pid_stat = fork();
    //        if (pid_stat == 0) {
    //            //            auto fd = ::open(filename_stat.c_str(), O_WRONLY);
    //            const std::string stat_events = hw_events + "," + cache_events + "," + syscall_events + "," + sched_events;
    //            //            ::dup2(fd, 1);
    //            //            dup2(fd, 2);
    //            exit(execl("/usr/bin/perf", "sudo perf", "stat", "-e", stat_events.c_str(), "-o", filename_stat.c_str(), "-p", s.str().c_str(), nullptr));
    //            //            exit(execl("/usr/bin/perf", "sudo perf", "stat", "-C0-5", "-A", "-e", stat_events.c_str(), "-o", filename_stat.c_str(), nullptr));
    //        }
    //        pid_t pid_rec = fork();
    //        if (pid_rec == 0) {
    //            //            auto fd = open("/dev/null", O_RDWR);
    //            //            dup2(fd, 1);
    //            //            dup2(fd, 2);
    //            exit(execl("/usr/bin/perf", "sudo perf", "record", "-g", "-e", hw_events.c_str(), "-o", filename_data.c_str(), "-p", s.str().c_str(), nullptr));
    //        }
    //
    //        // Run body
    //        body();
    //
    //        // Kill profiler  
    //        kill(pid_stat, SIGINT);
    //        waitpid(pid_stat, nullptr, 0);
    //
    //        // Kill profiler  
    //        kill(pid_rec, SIGINT);
    //        waitpid(pid_rec, nullptr, 0);
    //    }

};