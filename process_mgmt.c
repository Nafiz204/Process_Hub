#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <signal.h>
#include "process_mgmt.h"
#include "job_control.h"

pid_t fg_pid = 0;

void execute_external(char **args, int background) {
    sigset_t x;
    sigemptyset(&x);
    sigaddset(&x, SIGCHLD);
    sigprocmask(SIG_BLOCK, &x, NULL); // Block SIGCHLD

    pid_t pid = fork();

    if (pid == 0) {
        // Child process
        sigprocmask(SIG_UNBLOCK, &x, NULL); // Unblock in child
        
        // Reset signal handlers for child
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);

        if (execvp(args[0], args) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) {
        perror("fork");
        sigprocmask(SIG_UNBLOCK, &x, NULL);
    } else {
        // Parent process
        if (background) {
            add_job(pid, args[0], RUNNING);
            sigprocmask(SIG_UNBLOCK, &x, NULL);
        } else {
            fg_pid = pid;
            sigprocmask(SIG_UNBLOCK, &x, NULL);
            
            int status;
            // waitpid might be interrupted by signal, or child might already be reaped by SIGCHLD handler
            // If SIGCHLD handler reaped it, waitpid will return -1 with errno ECHILD.
            if (waitpid(pid, &status, WUNTRACED) > 0) {
                if (WIFSTOPPED(status)) {
                    add_job(pid, args[0], STOPPED);
                }
            }
            fg_pid = 0;
        }
    }
}

double get_system_uptime() {
    FILE *fp = fopen("/proc/uptime", "r");
    double uptime = 0;
    if (fp) {
        if (fscanf(fp, "%lf", &uptime) != 1) uptime = 0;
        fclose(fp);
    }
    return uptime;
}

void monitor_processes() {
    DIR *dir;
    struct dirent *entry;
    long hertz = sysconf(_SC_CLK_TCK);
    long page_size_kb = sysconf(_SC_PAGESIZE) / 1024;
    double uptime = get_system_uptime();

    printf("%-8s %-10s %-20s %-8s %-10s\n", "PID", "STATUS", "COMMAND", "CPU%", "MEM(kB)");
    
    dir = opendir("/proc");
    if (!dir) {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit(*entry->d_name)) continue;

        char path[512];
        // Get CPU and basic info from /proc/[pid]/stat
        snprintf(path, sizeof(path), "/proc/%s/stat", entry->d_name);
        FILE *fp = fopen(path, "r");
        if (!fp) continue;

        char buffer[1024];
        if (!fgets(buffer, sizeof(buffer), fp)) {
            fclose(fp);
            continue;
        }
        fclose(fp);

        // Parsing /proc/[pid]/stat
        // format: pid (name) state ppid pgrp session tty_nr tpgid flags minflt cminflt majflt cmajflt utime stime ...
        char *last_paren = strrchr(buffer, ')');
        if (!last_paren) continue;

        char state;
        unsigned long utime, stime;
        unsigned long long starttime;
        // After ')', there is a space, then state, then many other fields.
        // We need fields: 3 (state), 14 (utime), 15 (stime), 22 (starttime)
        // last_paren points to ')'. last_paren + 2 points to state.
        // After state, we want to skip ppid, pgrp, session, tty_nr, tpgid, flags, minflt, cminflt, majflt, cmajflt
        // That is 10 fields to skip.
        if (sscanf(last_paren + 2, "%c %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %lu %lu %*s %*s %*s %*s %*s %*s %llu", 
               &state, &utime, &stime, &starttime) < 4) continue;

        // Get command name from between the first '(' and last ')'
        char name[256] = "";
        char *first_paren = strchr(buffer, '(');
        if (first_paren && last_paren > first_paren) {
            int len = last_paren - first_paren - 1;
            if (len > 255) len = 255;
            strncpy(name, first_paren + 1, len);
            name[len] = '\0';
        }

        // Get Memory info from /proc/[pid]/statm
        snprintf(path, sizeof(path), "/proc/%s/statm", entry->d_name);
        fp = fopen(path, "r");
        long rss = 0;
        if (fp) {
            if (fscanf(fp, "%*s %ld", &rss) != 1) rss = 0;
            fclose(fp);
        }

        // CPU calculation
        double total_time = (double)(utime + stime) / hertz;
        double seconds = uptime - ((double)starttime / hertz);
        double cpu_usage = 0;
        if (seconds > 0) {
            cpu_usage = (total_time / seconds) * 100.0;
        }

        char state_str[20];
        switch (state) {
            case 'R': strcpy(state_str, "Running"); break;
            case 'S': strcpy(state_str, "Sleeping"); break;
            case 'D': strcpy(state_str, "Disk Sleep"); break;
            case 'Z': strcpy(state_str, "Zombie"); break;
            case 'T': strcpy(state_str, "Stopped"); break;
            case 't': strcpy(state_str, "Tracing"); break;
            case 'X':
            case 'x': strcpy(state_str, "Dead"); break;
            case 'K': strcpy(state_str, "Wakekill"); break;
            case 'W': strcpy(state_str, "Waking"); break;
            case 'P': strcpy(state_str, "Parked"); break;
            case 'I': strcpy(state_str, "Idle"); break;
            default: snprintf(state_str, sizeof(state_str), "Unknown(%c)", state); break;
        }

        printf("%-8s %-10s %-20s %-8.1f %-10ld\n", entry->d_name, state_str, name, cpu_usage, rss * page_size_kb);
    }
    closedir(dir);
}

void fg_job(int job_id) {
    Job *job = get_job_by_id(job_id);
    if (!job) {
        printf("fg: job [%d] not found\n", job_id);
        return;
    }

    if (job->status == STOPPED) {
        kill(job->pid, SIGCONT);
    }

    fg_pid = job->pid;
    printf("Bringing job [%d] to foreground: %s\n", job->job_id, job->command);
    
    int status;
    waitpid(job->pid, &status, WUNTRACED);
    
    if (WIFSTOPPED(status)) {
        update_job_status(job->pid, STOPPED);
    } else {
        remove_job(job->pid);
    }
    fg_pid = 0;
}

void bg_job(int job_id) {
    Job *job = get_job_by_id(job_id);
    if (!job) {
        printf("bg: job [%d] not found\n", job_id);
        return;
    }

    kill(job->pid, SIGCONT);
    update_job_status(job->pid, RUNNING);
    printf("[%d] %d Running %s &\n", job->job_id, job->pid, job->command);
}
