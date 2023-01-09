CC = gcc
CFLAGS += -g -Wall -O2

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)
DIR = build
OBJS := $(addprefix $(DIR)/, $(OBJS))

TARGET = enzyme

.PHONY: all clean

all: $(TARGET)

clean:
	rm $(OBJS)

$(DIR)/%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(TARGET): $(OBJS) | $(DIR)
	$(CC) -o $@ $^

$(DIR):
	mkdir $@
