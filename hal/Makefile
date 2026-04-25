# Biometric Vault — HAL Makefile
#
# Kullanım:
#   make              → libhal.a (statik kütüphane) + test programı derle
#   make test         → Test programını çalıştır (simulator çalışıyor olmalı!)
#   make clean        → Temizle
#
# Bağımlılıklar:
#   sudo apt install libcjson-dev libssl-dev

CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=c11 -g -O2
LDFLAGS = -lcjson -lssl -lcrypto

# Kaynak dosyalar
LIB_SRCS = hal_simulator.c hal_common.c
LIB_OBJS = $(LIB_SRCS:.c=.o)
LIB_NAME = libhal.a

TEST_SRC = test_hal.c
TEST_BIN = test_hal

.PHONY: all clean test

all: $(LIB_NAME) $(TEST_BIN)

# Statik kütüphane
$(LIB_NAME): $(LIB_OBJS)
	ar rcs $@ $^
	@echo "[HAL] Built $@"

# Test programı
$(TEST_BIN): $(TEST_SRC) $(LIB_NAME)
	$(CC) $(CFLAGS) -o $@ $< -L. -lhal $(LDFLAGS)
	@echo "[HAL] Built $@"

# Object dosyalar
%.o: %.c hal.h
	$(CC) $(CFLAGS) -c $< -o $@

# Test çalıştır
test: $(TEST_BIN)
	@echo ""
	@echo "NOTE: Make sure the simulator is running!"
	@echo "  cd ../hw_simulator && python3 simulator.py -v"
	@echo ""
	./$(TEST_BIN)

clean:
	rm -f $(LIB_OBJS) $(LIB_NAME) $(TEST_BIN)
	@echo "[HAL] Cleaned"
