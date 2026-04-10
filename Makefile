CC = gcc
CFLAGS = -Wall -Wextra -g
OBJ = job_control.o process_mgmt.o signals.o utils.o
TARGET = pm_shell
GUI_TARGET = pm_gui

# Manual GTK flags since pkg-config is missing
GTK_INC = -I/usr/include/gtk-3.0 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
          -I/usr/include/pango-1.0 -I/usr/include/cairo -I/usr/include/gdk-pixbuf-2.0 \
          -I/usr/include/atk-1.0 -I/usr/include/harfbuzz
GTK_LIB = -lgtk-3 -lgdk-3 -lpango-1.0 -lcairo -lgdk_pixbuf-2.0 -lgobject-2.0 -lglib-2.0

all: $(TARGET) $(GUI_TARGET)

$(TARGET): main.o $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) main.o $(OBJ)

$(GUI_TARGET): gui_main.o job_control.o process_mgmt_gui.o signals.o utils.o
	$(CC) $(CFLAGS) -o $(GUI_TARGET) gui_main.o job_control.o process_mgmt_gui.o signals.o utils.o $(GTK_LIB)

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

gui_main.o: gui_main.c
	$(CC) $(CFLAGS) $(GTK_INC) -DUSE_GTK -c gui_main.c

process_mgmt_gui.o: process_mgmt.c
	$(CC) $(CFLAGS) $(GTK_INC) -DUSE_GTK -c process_mgmt.c -o process_mgmt_gui.o

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(TARGET) $(GUI_TARGET)
