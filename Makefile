TARGET = kubsh
CC = gcc
CFLAGS = -Wall -Wextra -Werror -g -O2

SRCS = kubsh.c vfs.c
OBJS = $(SRCS:.c=.o)

LIBS = -lreadline -lfuse3 -lpthread

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)
