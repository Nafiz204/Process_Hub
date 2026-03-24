#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include "signals.h"
#include "job_control.h"

// Foreground process PID
extern pid_t fg_pid;

void sigint_handler(int sig) {
    (void)sig;
    if (fg_pid > 0) {
        kill(fg_pid, SIGINT);
    } else {
        printf("\npm_shell> ");
        fflush(stdout);
    }
}

void sigtstp_handler(int sig) {
    (void)sig;
    if (fg_pid > 0) {
        kill(fg_pid, SIGTSTP);
    } else {
        printf("\npm_shell> ");
        fflush(stdout);
    }
}

void sigchld_handler(int sig) {
    (void)sig;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            remove_job(pid);
        } else if (WIFSTOPPED(status)) {
            update_job_status(pid, STOPPED);
        } else if (WIFCONTINUED(status)) {
            update_job_status(pid, RUNNING);
        }
    }
}

void setup_signal_handlers() {
    struct sigaction sa_int, sa_tstp, sa_chld;

    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa_int, NULL);

    sa_tstp.sa_handler = sigtstp_handler;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa_tstp, NULL);

    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP; // We'll handle stops ourselves via waitpid
    // Actually, SA_NOCLDSTOP might interfere with WUNTRACED. Let's just use SA_RESTART.
    sa_chld.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa_chld, NULL);
}
