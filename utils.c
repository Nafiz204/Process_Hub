#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "utils.h"
#include "job_control.h"
#include "process_mgmt.h"

#define MAX_LINE 1024

int parse_command(char *line, char **args) {
    int i = 0;
    char *token = strtok(line, " \t\n");
    int background = 0;

    while (token != NULL) {
        if (strcmp(token, "&") == 0) {
            background = 1;
            break;
        }
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
    return background;
}

int is_builtin(char *cmd) {
    if (strcmp(cmd, "jobs") == 0 || strcmp(cmd, "fg") == 0 || 
        strcmp(cmd, "bg") == 0 || strcmp(cmd, "kill") == 0 || 
        strcmp(cmd, "stop") == 0 || strcmp(cmd, "resume") == 0 ||
        strcmp(cmd, "monitor") == 0) {
        return 1;
    }
    return 0;
}

void handle_builtin(char **args) {
    if (strcmp(args[0], "jobs") == 0) {
        list_jobs();
    } else if (strcmp(args[0], "fg") == 0) {
        if (args[1]) {
            int job_id = atoi(args[1]);
            fg_job(job_id);
        } else {
            printf("fg: usage: fg <job_id>\n");
        }
    } else if (strcmp(args[0], "bg") == 0) {
        if (args[1]) {
            int job_id = atoi(args[1]);
            bg_job(job_id);
        } else {
            printf("bg: usage: bg <job_id>\n");
        }
    } else if (strcmp(args[0], "kill") == 0) {
        if (args[1]) {
            pid_t pid = atoi(args[1]);
            if (kill(pid, SIGKILL) == -1) {
                perror("kill");
            }
        }
    } else if (strcmp(args[0], "stop") == 0) {
        if (args[1]) {
            pid_t pid = atoi(args[1]);
            if (kill(pid, SIGSTOP) == -1) {
                perror("stop");
            }
        }
    } else if (strcmp(args[0], "resume") == 0) {
        if (args[1]) {
            pid_t pid = atoi(args[1]);
            if (kill(pid, SIGCONT) == -1) {
                perror("resume");
            }
        }
    } else if (strcmp(args[0], "monitor") == 0) {
        monitor_processes();
    }
}

void shell_mode() {
    char line[MAX_LINE];
    char *args[MAX_LINE / 2 + 1];

    printf("\n--- Entering PM Shell Mode (type 'exit' to return to menu) ---\n");
    while (1) {
        printf("pm_shell> ");
        if (!fgets(line, MAX_LINE, stdin)) {
            break;
        }

        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        int background = parse_command(line, args);
        if (args[0] == NULL) continue;

        if (strcmp(args[0], "exit") == 0) {
            break;
        } else if (is_builtin(args[0])) {
            handle_builtin(args);
        } else {
            execute_external(args, background);
        }
    }
    printf("--- Exiting PM Shell Mode ---\n");
}

void job_management_menu() {
    printf("\n--- Job Management ---\n");
    list_jobs();
    printf("\nOptions:\n");
    printf("1. Bring job to Foreground\n");
    printf("2. Run job in Background\n");
    printf("3. Back to Main Menu\n");
    printf("Choice: ");

    int choice;
    if (scanf("%d", &choice) != 1) {
        while (getchar() != '\n'); // clear buffer
        return;
    }
    getchar(); // clear newline

    if (choice == 1 || choice == 2) {
        int job_id;
        printf("Enter Job ID: ");
        if (scanf("%d", &job_id) != 1) {
            while (getchar() != '\n');
            return;
        }
        getchar();
        if (choice == 1) fg_job(job_id);
        else bg_job(job_id);
    }
}

void process_control_menu() {
    printf("\n--- Process Control ---\n");
    int pid;
    printf("Enter PID to control: ");
    if (scanf("%d", &pid) != 1) {
        while (getchar() != '\n');
        return;
    }
    getchar();

    printf("Options:\n");
    printf("1. Kill (SIGKILL)\n");
    printf("2. Stop (SIGSTOP)\n");
    printf("3. Resume (SIGCONT)\n");
    printf("4. Back\n");
    printf("Choice: ");

    int choice;
    if (scanf("%d", &choice) != 1) {
        while (getchar() != '\n');
        return;
    }
    getchar();

    switch (choice) {
        case 1: kill(pid, SIGKILL); break;
        case 2: kill(pid, SIGSTOP); break;
        case 3: kill(pid, SIGCONT); break;
        default: break;
    }
}
