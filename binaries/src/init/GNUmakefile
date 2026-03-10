CC := gcc
SRC_DIR := .
INITRD := ../../../initrd
OUT := init
CFLAGS := -g
LDFLAGS := --sysroot=$(SYSROOT) -lc
EXTRA_FLAGS :=

all: compile

compile:
	@echo "Building init"
	@mkdir -p $(INITRD)
	@$(CC) $(SRC_DIR)/*.c $(CFLAGS) $(EXTRA_FLAGS) -o $(INITRD)/$(OUT) $(LDFLAGS)
	@echo "Built $(INITRD)/$(OUT)"

clean:
	@rm -rf $(INITRD)/*
	@echo "Cleaned initrd folder"

.PHONY: compile clean all
