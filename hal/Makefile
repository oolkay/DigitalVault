CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g -O2
LDFLAGS = -lcjson -lssl -lcrypto

LIB_SRCS = hal_core.c hal_simulator.c hal_serial.c hal_protocol.c hal_common.c
LIB_OBJS = $(LIB_SRCS:.c=.o)
LIB_NAME = libhal.a
TEST_BIN = test_hal

.PHONY: all clean test

all: $(LIB_NAME) $(TEST_BIN)

$(LIB_NAME): $(LIB_OBJS)
	ar rcs $@ $^

$(TEST_BIN): test_hal.c $(LIB_NAME)
	$(CC) $(CFLAGS) -o $@ test_hal.c -L. -lhal $(LDFLAGS)

%.o: %.c hal.h hal_protocol.h hal_internal.h
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(LIB_OBJS) $(LIB_NAME) $(TEST_BIN)
