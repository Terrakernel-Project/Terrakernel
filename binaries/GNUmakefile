INIT_FLAGS := -ffreestanding -nolibc -nostdlib -isystem ./__sys/ -I ./__sys/ -e HlMain

# INIT_LINKER := -T $(INIT_LINKER_SCRIPT)
INIT_LINKER := -pie

SRC_DIR := ./src
INITRD := ../initrd

DIRS := $(shell find $(SRC_DIR) -mindepth 1 -maxdepth 1 -type d)

EXES := $(patsubst $(SRC_DIR)/%, $(INITRD)/%, $(DIRS))

all: $(EXES)

$(INITRD)/%: $(SRC_DIR)/%/*.c
	@mkdir -p $(INITRD)
	@gcc $^ -fPIE -o $@ $(INIT_FLAGS) $(INIT_LINKER)
	@echo "Created executable $@ from folder $*"

.PHONY: all clean

clean:
	@rm -f $(INITRD)/*
	@echo "Cleaned initrd folder"
