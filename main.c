#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "utils.h"
#include "job_control.h"
#include "signals.h"
#include "process_mgmt.h"

void display_main_menu() {
    printf("\n========== Process Manager Shell ==========\n");
    printf("1. Monitor Processes (Task Manager)\n");
    printf("2. Job Management (fg/bg)\n");
    printf("3. Process Control (Signals)\n");
    printf("4. PM Shell Mode (Command Line)\n");
    printf("5. Exit\n");
    printf("===========================================\n");
    printf("Choice: ");
}

int main() {
    // Initialize job control and signal handling
    init_job_control();
    setup_signal_handlers();

    int choice;
    while (1) {
        display_main_menu();
        if (scanf("%d", &choice) != 1) {
            // Clear invalid input
            while (getchar() != '\n');
            printf("Invalid input. Please enter a number.\n");
            continue;
        }
        getchar(); // consume newline

        switch (choice) {
            case 1:
                monitor_processes();
                break;
            case 2:
                job_management_menu();
                break;
            case 3:
                process_control_menu();
                break;
            case 4:
                shell_mode();
                break;
            case 5:
                printf("Exiting...\n");
                return 0;
            default:
                printf("Invalid choice. Please try again.\n");
                break;
        }
    }

    return 0;
}
