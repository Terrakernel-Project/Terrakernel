CC := gcc
SRC_DIR := .
INITRD := ../../../initrd
OUT := init
LIB_DIR := ../../../imginc/fs/Terra/Terrakernel/Sys64/Libs/
CFLAGS := -g -ffreestanding -fPIC -isystem ../../__sys/ -I ../../__sys/
LDFLAGS := -e HlMain -Wl,-dynamic-linker=/lib64/ld-linux-x86-64.so.2 -Wl,--no-as-needed -L$(LIB_DIR) -Bdynamic -ltest -Bstatic

all: compile

compile:
	@echo "Building init with dynamic libtest.so reference"
	@mkdir -p $(INITRD)
	@$(CC) $(SRC_DIR)/*.c $(CFLAGS) -nostartfiles -o $(INITRD)/$(OUT) $(LDFLAGS)
	@echo "Built $(INITRD)/$(OUT)"

clean:
	@rm -rf $(INITRD)/*
	@echo "Cleaned initrd folder"

.PHONY: compile clean all
