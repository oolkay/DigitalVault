# Biometric Vault — FUSE Makefile

CC      = gcc
CFLAGS  = -Wall -Wextra -g -O2 -D_FILE_OFFSET_BITS=64
CFLAGS += $(shell pkg-config fuse3 --cflags)
LDFLAGS = $(shell pkg-config fuse3 --libs) -lcjson -lssl -lcrypto

# HAL ve Crypto dosyalarına erişim için
INCLUDES = -I. -I../hal
HAL_DIR  = ../hal

SRCS = vault_fs.c block_io.c crypto.c $(HAL_DIR)/hal_simulator.c $(HAL_DIR)/hal_common.c
OBJS = $(SRCS:.c=.o)
TARGET = vault_fs

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)
	@echo "[FUSE] Built $(TARGET)"

clean:
	rm -f $(TARGET) *.o
	@echo "[FUSE] Cleaned"
