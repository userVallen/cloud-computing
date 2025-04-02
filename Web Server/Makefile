CC = gcc
CFLAGS = -Wall
TARGET = main
SRCS = main.c
OBJS = main.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(FLAGS) $(OBJS) -o $(TARGET)

%.o: %.c
	$(CC) $(FLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *.o