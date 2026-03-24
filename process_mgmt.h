#ifndef PROCESS_MGMT_H
#define PROCESS_MGMT_H

#include <sys/types.h>

void execute_external(char **args, int background);
void monitor_processes();
void fg_job(int job_id);
void bg_job(int job_id);

#endif
