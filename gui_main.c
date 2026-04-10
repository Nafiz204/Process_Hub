#include <gtk/gtk.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include "process_mgmt.h"
#include "job_control.h"
#include "signals.h"
#include "utils.h"

enum {
    SYS_PID_COL = 0,
    SYS_USER_COL,
    SYS_STATUS_COL,
    SYS_COMMAND_COL,
    SYS_PPID_COL,
    SYS_THREADS_COL,
    SYS_PRIO_COL,
    SYS_CPU_COL,
    SYS_MEM_COL,
    SYS_SEEN_COL,
    SYS_NUM_COLS
};

enum {
    JOB_ID_COL = 0,
    JOB_PID_COL,
    JOB_STATUS_COL,
    JOB_COMMAND_COL,
    JOB_SEEN_COL,
    JOB_NUM_COLS
};

typedef struct {
    GtkListStore *sys_store;
    GtkListStore *job_store;
    GtkTreeView  *sys_tree;
    GtkTreeView  *job_tree;
    GtkWidget    *window;
    GtkTextBuffer *output_buffer;
} AppWidgets;

static void update_job_list(GtkListStore *store) {
    // Mark all as NOT seen
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
    while (valid) {
        gtk_list_store_set(store, &iter, JOB_SEEN_COL, FALSE, -1);
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
    }

    Job *curr = get_job_list_head();
    while (curr != NULL) {
        char *status_str = (curr->status == RUNNING ? "Running" : "Stopped");
        gboolean found = FALSE;
        valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
        while (valid) {
            int existing_id;
            gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, JOB_ID_COL, &existing_id, -1);
            if (existing_id == curr->job_id) {
                gtk_list_store_set(store, &iter,
                                   JOB_PID_COL, curr->pid,
                                   JOB_STATUS_COL, status_str,
                                   JOB_COMMAND_COL, curr->command,
                                   JOB_SEEN_COL, TRUE, -1);
                found = TRUE;
                break;
            }
            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
        }

        if (!found) {
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter,
                               JOB_ID_COL, curr->job_id,
                               JOB_PID_COL, curr->pid,
                               JOB_STATUS_COL, status_str,
                               JOB_COMMAND_COL, curr->command,
                               JOB_SEEN_COL, TRUE, -1);
        }
        curr = curr->next;
    }

    // Remove those not seen
    valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
    while (valid) {
        gboolean seen;
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, JOB_SEEN_COL, &seen, -1);
        if (!seen) {
            if (!gtk_list_store_remove(store, &iter)) break;
        } else {
            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
        }
    }
}

static void save_and_restore_selection(GtkTreeView *tree_view, int pid_col) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    GtkTreeIter iter;
    gint selected_pid = -1;

    // 1. Save scroll position
    GtkAdjustment *adj = gtk_tree_view_get_vadjustment(tree_view);
    double scroll_val = gtk_adjustment_get_value(adj);

    // 2. Save selected PID
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter, pid_col, &selected_pid, -1);
    }

    // 3. Refresh model data
    if (pid_col == SYS_PID_COL) {
        monitor_processes_gui(GTK_LIST_STORE(model));
    } else {
        update_job_list(GTK_LIST_STORE(model));
    }

    // 4. Restore selection by PID
    if (selected_pid != -1) {
        if (gtk_tree_model_get_iter_first(model, &iter)) {
            do {
                gint pid;
                gtk_tree_model_get(model, &iter, pid_col, &pid, -1);
                if (pid == selected_pid) {
                    gtk_tree_selection_select_iter(selection, &iter);
                    break;
                }
            } while (gtk_tree_model_iter_next(model, &iter));
        }
    }

    // 5. Restore scroll position
    gtk_adjustment_set_value(adj, scroll_val);
}

static gboolean on_refresh_timeout(gpointer data) {
    AppWidgets *widgets = (AppWidgets *)data;
    save_and_restore_selection(widgets->sys_tree, SYS_PID_COL);
    save_and_restore_selection(widgets->job_tree, JOB_PID_COL);
    return TRUE;
}

static gboolean on_io_data(GIOChannel *source, GIOCondition condition, gpointer data) {
    GtkTextBuffer *buffer = GTK_TEXT_BUFFER(data);
    if (condition & G_IO_IN) {
        char buf[1024];
        gsize bytes_read;
        GIOStatus status = g_io_channel_read_chars(source, buf, sizeof(buf) - 1, &bytes_read, NULL);
        if (status == G_IO_STATUS_NORMAL && bytes_read > 0) {
            buf[bytes_read] = '\0';
            GtkTextIter end;
            gtk_text_buffer_get_end_iter(buffer, &end);
            gtk_text_buffer_insert(buffer, &end, buf, -1);
        }
    }
    if (condition & (G_IO_HUP | G_IO_ERR)) {
        g_io_channel_unref(source);
        return FALSE; 
    }
    return TRUE;
}

static void on_fg_dialog_response(GtkDialog *dialog, gint response_id, gpointer data) {
    pid_t pid = (pid_t)GPOINTER_TO_INT(data);
    if (response_id == 2) { 
        kill(pid, SIGKILL);
    }
    gtk_widget_hide(GTK_WIDGET(dialog));
}

static void run_foreground_wait(AppWidgets *widgets, pid_t pid, const char *cmd) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Waiting for process...",
                                                    GTK_WINDOW(widgets->window),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "Send to Background", 1,
                                                    "Kill Process", 2,
                                                    NULL);
    
    GtkWidget *dialog_ptr = dialog;
    g_signal_connect(dialog, "destroy", G_CALLBACK(gtk_widget_destroyed), &dialog_ptr);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    char msg[512];
    snprintf(msg, sizeof(msg), "\nRunning: %s\n\n(PID: %d)\n\n"
                               "You can send this task to the background\n"
                               "to continue using the GUI, or kill it.", cmd, pid);
    GtkWidget *label = gtk_label_new(msg);
    gtk_container_add(GTK_CONTAINER(content), label);
    gtk_widget_show_all(dialog);

    extern pid_t fg_pid;
    fg_pid = pid;

    g_signal_connect(dialog, "response", G_CALLBACK(on_fg_dialog_response), GINT_TO_POINTER(pid));

    while (dialog_ptr != NULL) {
        int status;
        pid_t result = waitpid(pid, &status, WNOHANG | WUNTRACED);
        
        if (result == -1 && errno != EINTR) break; 
        if (result > 0) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) break; 
            if (WIFSTOPPED(status)) break; 
        }

        while (gtk_events_pending()) gtk_main_iteration();
        if (dialog_ptr && !gtk_widget_get_visible(dialog_ptr)) break;
        usleep(50000); 
    }

    fg_pid = 0;
    if (dialog_ptr != NULL) {
        gtk_widget_destroy(dialog_ptr);
    }
}

static void on_signal_clicked(GtkButton *button, gpointer data) {
    AppWidgets *widgets = (AppWidgets *)data;
    const char *label = gtk_button_get_label(button);
    
    GtkNotebook *notebook = GTK_NOTEBOOK(g_object_get_data(G_OBJECT(widgets->sys_tree), "notebook"));
    gint page_num = gtk_notebook_get_current_page(notebook);
    GtkTreeView *tree_view = (page_num == 0) ? widgets->sys_tree : widgets->job_tree;
    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gint pid;
        int pid_idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(tree_view), "pid_col"));
        gtk_tree_model_get(model, &iter, pid_idx, &pid, -1);
        
        if (strcmp(label, "FG") == 0) {
            if (page_num == 1) {
                char *cmd_ptr;
                gtk_tree_model_get(model, &iter, JOB_COMMAND_COL, &cmd_ptr, -1);
                run_foreground_wait(widgets, pid, cmd_ptr);
                g_free(cmd_ptr);
            }
            return;
        }

        int sig = SIGKILL;
        if (strcmp(label, "Stop") == 0) sig = SIGSTOP;
        else if (strcmp(label, "Resume") == 0) sig = SIGCONT;

        if (pid > 0) {
            if (kill(pid, sig) != 0) perror("kill");
        }
    }
}

static void on_run_command(GtkWidget *widget, gpointer data) {
    AppWidgets *widgets = (AppWidgets *)data;
    GtkEntry *entry = GTK_IS_ENTRY(widget) ? GTK_ENTRY(widget) : GTK_ENTRY(g_object_get_data(G_OBJECT(widget), "entry"));
    const char *cmd = gtk_entry_get_text(entry);
    if (strlen(cmd) == 0) return;

    char cmd_copy[1024];
    strncpy(cmd_copy, cmd, 1023);
    cmd_copy[1023] = '\0';

    char *args[512];
    int background = parse_command(cmd_copy, args);

    if (args[0] == NULL) return;

    if (is_builtin(args[0])) {
        handle_builtin(args);
    } else {
        int out_fd = -1;
        pid_t pid = execute_external(args, background, &out_fd);
        if (pid > 0 && out_fd != -1) {
            GIOChannel *channel = g_io_channel_unix_new(out_fd);
            g_io_channel_set_encoding(channel, NULL, NULL);
            g_io_add_watch(channel, G_IO_IN | G_IO_HUP | G_IO_ERR, on_io_data, widgets->output_buffer);
            
            char log[256];
            snprintf(log, sizeof(log), "\n>>> Running: %s\n", cmd);
            GtkTextIter end;
            gtk_text_buffer_get_end_iter(widgets->output_buffer, &end);
            gtk_text_buffer_insert(widgets->output_buffer, &end, log, -1);
        }
        if (!background && pid > 0) {
            run_foreground_wait(widgets, pid, args[0]);
        }
    }
    
    gtk_entry_set_text(entry, "");
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    init_job_control();
    setup_signal_handlers();

    AppWidgets *widgets = g_new0(AppWidgets, 1);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    widgets->window = window;
    gtk_window_set_title(GTK_WINDOW(window), "Process Hub");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    GtkWidget *hbox_run = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_run, FALSE, FALSE, 0);
    
    GtkWidget *lbl_run = gtk_label_new("Run Command:");
    gtk_box_pack_start(GTK_BOX(hbox_run), lbl_run, FALSE, FALSE, 0);

    GtkWidget *entry_run = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_run), "e.g. ls -l or top");
    gtk_box_pack_start(GTK_BOX(hbox_run), entry_run, TRUE, TRUE, 0);
    g_signal_connect(entry_run, "activate", G_CALLBACK(on_run_command), widgets);

    GtkWidget *btn_run = gtk_button_new_with_label("Run");
    g_object_set_data(G_OBJECT(btn_run), "entry", entry_run);
    gtk_box_pack_start(GTK_BOX(hbox_run), btn_run, FALSE, FALSE, 0);
    g_signal_connect(btn_run, "clicked", G_CALLBACK(on_run_command), widgets);

    GtkWidget *notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

    // --- System Tab ---
    widgets->sys_store = gtk_list_store_new(SYS_NUM_COLS, 
                                            G_TYPE_INT,    // PID
                                            G_TYPE_STRING, // USER
                                            G_TYPE_STRING, // STATUS
                                            G_TYPE_STRING, // COMMAND
                                            G_TYPE_INT,    // PPID
                                            G_TYPE_INT,    // THREADS
                                            G_TYPE_INT,    // PRIO
                                            G_TYPE_DOUBLE, // CPU%
                                            G_TYPE_LONG,   // MEM
                                            G_TYPE_BOOLEAN); // SEEN

    widgets->sys_tree = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(widgets->sys_store)));
    g_object_set_data(G_OBJECT(widgets->sys_tree), "store", widgets->sys_store);
    g_object_set_data(G_OBJECT(widgets->sys_tree), "pid_col", GINT_TO_POINTER(SYS_PID_COL));
    g_object_set_data(G_OBJECT(widgets->sys_tree), "notebook", notebook);

    const char *sys_headers[] = {"PID", "User", "Status", "Command", "PPID", "Threads", "Prio", "CPU%", "Mem(kB)"};
    for (int i = 0; i < SYS_NUM_COLS - 1; i++) {
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(sys_headers[i], renderer, "text", i, NULL);
        gtk_tree_view_column_set_sort_column_id(column, i);
        gtk_tree_view_append_column(widgets->sys_tree, column);
    }
    GtkWidget *sys_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(sys_scroll), GTK_WIDGET(widgets->sys_tree));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), sys_scroll, gtk_label_new("System Processes"));

    // --- Jobs Tab ---
    widgets->job_store = gtk_list_store_new(JOB_NUM_COLS, 
                                            G_TYPE_INT,    // ID
                                            G_TYPE_INT,    // PID
                                            G_TYPE_STRING, // STATUS
                                            G_TYPE_STRING, // CMD
                                            G_TYPE_BOOLEAN); // SEEN
    widgets->job_tree = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(widgets->job_store)));
    g_object_set_data(G_OBJECT(widgets->job_tree), "store", widgets->job_store);
    g_object_set_data(G_OBJECT(widgets->job_tree), "pid_col", GINT_TO_POINTER(JOB_PID_COL));

    const char *job_headers[] = {"Job ID", "PID", "Status", "Command"};
    for (int i = 0; i < JOB_NUM_COLS - 1; i++) {
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(job_headers[i], renderer, "text", i, NULL);
        gtk_tree_view_append_column(widgets->job_tree, column);
    }
    GtkWidget *job_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(job_scroll), GTK_WIDGET(widgets->job_tree));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), job_scroll, gtk_label_new("Shell Jobs"));

    // --- Output Log Tab ---
    GtkWidget *output_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(output_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(output_view), FALSE);
    widgets->output_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(output_view));
    GtkWidget *output_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(output_scroll), output_view);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), output_scroll, gtk_label_new("Command Output"));

    GtkWidget *hbox_ctrl = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_ctrl, FALSE, FALSE, 0);

    GtkWidget *kill_btn = gtk_button_new_with_label("Kill");
    GtkWidget *stop_btn = gtk_button_new_with_label("Stop");
    GtkWidget *cont_btn = gtk_button_new_with_label("Resume");
    GtkWidget *fg_btn   = gtk_button_new_with_label("FG");

    gtk_box_pack_start(GTK_BOX(hbox_ctrl), kill_btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_ctrl), stop_btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_ctrl), cont_btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_ctrl), fg_btn, TRUE, TRUE, 0);

    g_signal_connect(kill_btn, "clicked", G_CALLBACK(on_signal_clicked), widgets);
    g_signal_connect(stop_btn, "clicked", G_CALLBACK(on_signal_clicked), widgets);
    g_signal_connect(cont_btn, "clicked", G_CALLBACK(on_signal_clicked), widgets);
    g_signal_connect(fg_btn,   "clicked", G_CALLBACK(on_signal_clicked), widgets);

    monitor_processes_gui(widgets->sys_store);
    update_job_list(widgets->job_store);
    g_timeout_add(1000, on_refresh_timeout, widgets);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
