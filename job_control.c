#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "job_control.h"

static Job *head = NULL;
static int next_job_id = 1;

void init_job_control() {
    head = NULL;
    next_job_id = 1;
}

void add_job(pid_t pid, char *command, job_status status) {
    Job *new_job = (Job *)malloc(sizeof(Job));
    new_job->job_id = next_job_id++;
    new_job->pid = pid;
    strncpy(new_job->command, command, 255);
    new_job->status = status;
    new_job->next = head;
    head = new_job;

    printf("[%d] %d %s %s\n", new_job->job_id, new_job->pid, 
           (new_job->status == RUNNING ? "Running" : "Stopped"), new_job->command);
}

void remove_job(pid_t pid) {
    Job *curr = head;
    Job *prev = NULL;

    while (curr != NULL) {
        if (curr->pid == pid) {
            if (prev == NULL) {
                head = curr->next;
            } else {
                prev->next = curr->next;
            }
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

void update_job_status(pid_t pid, job_status status) {
    Job *curr = head;
    while (curr != NULL) {
        if (curr->pid == pid) {
            curr->status = status;
            return;
        }
        curr = curr->next;
    }
}

void list_jobs() {
    Job *curr = head;
    while (curr != NULL) {
        printf("[%d] %d %s %s\n", curr->job_id, curr->pid, 
               (curr->status == RUNNING ? "Running" : "Stopped"), curr->command);
        curr = curr->next;
    }
}

Job* get_job_by_id(int job_id) {
    Job *curr = head;
    while (curr != NULL) {
        if (curr->job_id == job_id) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

Job* get_job_by_pid(pid_t pid) {
    Job *curr = head;
    while (curr != NULL) {
        if (curr->pid == pid) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}
