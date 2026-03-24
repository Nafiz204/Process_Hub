CC = gcc
CFLAGS = -Wall -Wextra -g
OBJ = main.o job_control.o process_mgmt.o signals.o utils.o
TARGET = pm_shell

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(TARGET)
