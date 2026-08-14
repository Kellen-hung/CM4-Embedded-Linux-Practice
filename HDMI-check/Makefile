CC = gcc
CFLAGS = -Wall -Wextra
LIBS = -lgpiod

TARGET = hdmi-monitor
SRCS = main.c gpio.c hdmi.c uevent.c
OBJS = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)