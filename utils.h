#ifndef UTILS_H
#define UTILS_H

int parse_command(char *line, char **args);
int is_builtin(char *cmd);
void handle_builtin(char **args);
void shell_mode();
void job_management_menu();
void process_control_menu();

#endif
