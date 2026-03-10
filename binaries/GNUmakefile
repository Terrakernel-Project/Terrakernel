PREFIX := ./src/
INIT := $(PREFIX)init/
SHARED_FLAGS_CC := \
	--sysroot=../../resources/ \
	-static

compile_all:
	@$(MAKE) -C $(INIT) "EXTRA_FLAGS+=$(SHARED_FLAGS_CC)"
