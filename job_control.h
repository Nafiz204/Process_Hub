#ifndef JOB_CONTROL_H
#define JOB_CONTROL_H

#include <sys/types.h>

typedef enum {
    RUNNING,
    STOPPED,
    TERMINATED
} job_status;

typedef struct job {
    int job_id;
    pid_t pid;
    char command[256];
    job_status status;
    struct job *next;
} Job;

void init_job_control();
void add_job(pid_t pid, char *command, job_status status);
void remove_job(pid_t pid);
void update_job_status(pid_t pid, job_status status);
void list_jobs();
Job* get_job_by_id(int job_id);
Job* get_job_by_pid(pid_t pid);
Job* get_job_list_head();

#endif
