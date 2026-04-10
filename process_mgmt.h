#ifndef PROCESS_MGMT_H
#define PROCESS_MGMT_H

#include <sys/types.h>

pid_t execute_external(char **args, int background, int *out_fd);
void monitor_processes();

#ifdef USE_GTK
#include <gtk/gtk.h>
void monitor_processes_gui(GtkListStore *store);
#endif

void fg_job(int job_id);
void bg_job(int job_id);

#endif
