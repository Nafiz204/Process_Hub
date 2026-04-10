#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <signal.h>
#include <pwd.h>
#include <sys/stat.h>
#include <errno.h>
#include "process_mgmt.h"
#include "job_control.h"

pid_t fg_pid = 0;

static char* get_username_by_uid(uid_t uid) {
    struct passwd *pw = getpwuid(uid);
    if (pw) return pw->pw_name;
    return "unknown";
}

pid_t execute_external(char **args, int background, int *out_fd) {
    sigset_t x;
    sigemptyset(&x);
    sigaddset(&x, SIGCHLD);
    sigprocmask(SIG_BLOCK, &x, NULL); 

    int p[2];
    if (out_fd) {
        if (pipe(p) == -1) {
            perror("pipe");
            sigprocmask(SIG_UNBLOCK, &x, NULL);
            return -1;
        }
    }

    pid_t pid = fork();

    if (pid == 0) {
        sigprocmask(SIG_UNBLOCK, &x, NULL); 
        
        if (out_fd) {
            close(p[0]); 
            dup2(p[1], STDOUT_FILENO); 
            dup2(p[1], STDERR_FILENO); 
            close(p[1]);
        }

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
        if (out_fd) { close(p[0]); close(p[1]); }
        sigprocmask(SIG_UNBLOCK, &x, NULL);
    } else {
        if (out_fd) {
            close(p[1]); 
            *out_fd = p[0]; 
        }
        
        if (background) {
            add_job(pid, args[0], RUNNING);
            sigprocmask(SIG_UNBLOCK, &x, NULL);
        } else {
            fg_pid = pid;
            sigprocmask(SIG_UNBLOCK, &x, NULL);
            
#ifndef USE_GTK
            int status;
            if (waitpid(pid, &status, WUNTRACED) > 0) {
                if (WIFSTOPPED(status)) {
                    add_job(pid, args[0], STOPPED);
                }
            }
            fg_pid = 0;
#else
            add_job(pid, args[0], RUNNING);
#endif
        }
    }
    return pid;
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
    if (!dir) return;

    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit(*entry->d_name)) continue;

        char path[512];
        snprintf(path, sizeof(path), "/proc/%s/stat", entry->d_name);
        FILE *fp = fopen(path, "r");
        if (!fp) continue;

        char buffer[1024];
        if (!fgets(buffer, sizeof(buffer), fp)) {
            fclose(fp);
            continue;
        }
        fclose(fp);

        char *last_paren = strrchr(buffer, ')');
        if (!last_paren) continue;

        char state;
        unsigned long utime, stime;
        unsigned long long starttime;
        if (sscanf(last_paren + 2, "%c %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %lu %lu %*s %*s %*s %*s %*s %*s %llu", 
               &state, &utime, &stime, &starttime) < 4) continue;

        char name[256] = "";
        char *first_paren = strchr(buffer, '(');
        if (first_paren && last_paren > first_paren) {
            int len = last_paren - first_paren - 1;
            if (len > 255) len = 255;
            strncpy(name, first_paren + 1, len);
            name[len] = '\0';
        }

        snprintf(path, sizeof(path), "/proc/%s/statm", entry->d_name);
        fp = fopen(path, "r");
        long rss = 0;
        if (fp) {
            if (fscanf(fp, "%*s %ld", &rss) != 1) rss = 0;
            fclose(fp);
        }

        double total_time = (double)(utime + stime) / hertz;
        double seconds = uptime - ((double)starttime / hertz);
        double cpu_usage = 0;
        if (seconds > 0) cpu_usage = (total_time / seconds) * 100.0;

        char state_str[20];
        switch (state) {
            case 'R': strcpy(state_str, "Running"); break;
            case 'S': strcpy(state_str, "Sleeping"); break;
            case 'D': strcpy(state_str, "Disk Sleep"); break;
            case 'Z': strcpy(state_str, "Zombie"); break;
            case 'T': strcpy(state_str, "Stopped"); break;
            default: snprintf(state_str, sizeof(state_str), "Unknown(%c)", state); break;
        }

        printf("%-8s %-10s %-20s %-8.1f %-10ld\n", entry->d_name, state_str, name, cpu_usage, rss * page_size_kb);
    }
    closedir(dir);
}

#ifdef USE_GTK
#include <gtk/gtk.h>

void monitor_processes_gui(GtkListStore *store) {
    DIR *dir;
    struct dirent *entry;
    long hertz = sysconf(_SC_CLK_TCK);
    long page_size_kb = sysconf(_SC_PAGESIZE) / 1024;
    double uptime = get_system_uptime();

    GHashTable *pid_map = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
    while (valid) {
        int pid;
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 0, &pid, -1);
        
        // Mark as NOT seen (SYS_SEEN_COL is 9)
        gtk_list_store_set(store, &iter, 9, FALSE, -1);
        
        GtkTreeIter *piter = g_memdup(&iter, sizeof(GtkTreeIter));
        g_hash_table_insert(pid_map, GINT_TO_POINTER(pid), piter);
        
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
    }

    dir = opendir("/proc");
    if (!dir) {
        g_hash_table_destroy(pid_map);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit(*entry->d_name)) continue;

        int pid = atoi(entry->d_name);
        char path[512];
        struct stat st;
        snprintf(path, sizeof(path), "/proc/%s", entry->d_name);
        char *user = "unknown";
        if (stat(path, &st) == 0) user = get_username_by_uid(st.st_uid);

        snprintf(path, sizeof(path), "/proc/%s/stat", entry->d_name);
        FILE *fp = fopen(path, "r");
        if (!fp) continue;

        char buffer[1024];
        if (!fgets(buffer, sizeof(buffer), fp)) { fclose(fp); continue; }
        fclose(fp);

        char *last_paren = strrchr(buffer, ')');
        if (!last_paren) continue;

        char state;
        unsigned long utime, stime;
        long ppid, threads, priority;
        unsigned long long starttime;
        if (sscanf(last_paren + 2, "%c %ld %*s %*s %*s %*s %*s %*s %*s %*s %*s %lu %lu %*s %*s %ld %*s %ld %*s %llu", 
               &state, &ppid, &utime, &stime, &priority, &threads, &starttime) < 7) continue;

        char name[256] = "";
        char *first_paren = strchr(buffer, '(');
        if (first_paren && last_paren > first_paren) {
            int len = last_paren - first_paren - 1;
            if (len > 255) len = 255;
            strncpy(name, first_paren + 1, len);
            name[len] = '\0';
        }

        snprintf(path, sizeof(path), "/proc/%s/statm", entry->d_name);
        fp = fopen(path, "r");
        long rss = 0;
        if (fp) { fscanf(fp, "%*s %ld", &rss); fclose(fp); }

        double total_time = (double)(utime + stime) / hertz;
        double seconds = uptime - ((double)starttime / hertz);
        double cpu_usage = (seconds > 0) ? (total_time / seconds) * 100.0 : 0;

        char state_str[20];
        switch (state) {
            case 'R': strcpy(state_str, "Running"); break;
            case 'S': strcpy(state_str, "Sleeping"); break;
            case 'D': strcpy(state_str, "Disk Sleep"); break;
            case 'Z': strcpy(state_str, "Zombie"); break;
            case 'T': strcpy(state_str, "Stopped"); break;
            default: snprintf(state_str, sizeof(state_str), "%c", state); break;
        }

        GtkTreeIter *existing_iter = g_hash_table_lookup(pid_map, GINT_TO_POINTER(pid));
        if (existing_iter) {
            gtk_list_store_set(store, existing_iter,
                               1, user, 2, state_str, 3, name,
                               4, (int)ppid, 5, (int)threads, 6, (int)priority,
                               7, cpu_usage, 8, rss * page_size_kb,
                               9, TRUE, -1);
        } else {
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter,
                               0, pid, 1, user, 2, state_str, 3, name,
                               4, (int)ppid, 5, (int)threads, 6, (int)priority,
                               7, cpu_usage, 8, rss * page_size_kb,
                               9, TRUE, -1);
        }
    }
    closedir(dir);
    g_hash_table_destroy(pid_map);

    valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
    while (valid) {
        gboolean seen;
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 9, &seen, -1);
        if (!seen) {
            if (!gtk_list_store_remove(store, &iter)) break;
        } else {
            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
        }
    }
}
#endif

void fg_job(int job_id) {
    Job *job = get_job_by_id(job_id);
    if (!job) return;
    if (job->status == STOPPED) kill(job->pid, SIGCONT);
    fg_pid = job->pid;
    int status;
    waitpid(job->pid, &status, WUNTRACED);
    if (WIFSTOPPED(status)) update_job_status(job->pid, STOPPED);
    else remove_job(job->pid);
    fg_pid = 0;
}

void bg_job(int job_id) {
    Job *job = get_job_by_id(job_id);
    if (!job) return;
    kill(job->pid, SIGCONT);
    update_job_status(job->pid, RUNNING);
}
