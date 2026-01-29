INIT_FLAGS := -g -ffreestanding -fno-pie -nostdlib -nostartfiles -Wl,-Ttext=0x401000 -e HlMain -isystem ./__sys/ -I ./__sys/

INIT_LINKER := -no-pie
# INIT_LINKER := -pie

SRC_DIR := ./src
INITRD := ../initrd
# RELOC := -fPIE
RELOC := -fno-pie

DIRS := $(shell find $(SRC_DIR) -mindepth 1 -maxdepth 1 -type d)

EXES := $(patsubst $(SRC_DIR)/%, $(INITRD)/%, $(DIRS))

all: $(EXES)

$(INITRD)/%: $(SRC_DIR)/%/*.c
	@mkdir -p $(INITRD)
	@gcc $^ $(RELOC) -o $@ $(INIT_FLAGS) $(INIT_LINKER)
	@echo "Created executable $@ from folder $*"

.PHONY: all clean

clean:
	@rm -f $(INITRD)/*
	@echo "Cleaned initrd folder"
