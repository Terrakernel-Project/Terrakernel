#include <arch/x86_64/syscall/handlers.hpp>
#include "syscalls.hpp"

int64_t no_handler() {
	return 0;
}

void register_syscall(uint64_t vector,
					void* handler,
					uint64_t num_args,
					const char* name,
					const char* cpp_pretty_func);

void set_no_handler(uint64_t vector) {
	register_syscall(vector, (void*)no_handler, 0, "<N/A>", "<N/A> <N/A>(<N/A>)");
}

#define set_handler(vec, func, nargs) \
	register_syscall(vec, (void*)func, nargs, "#func#", "C++ pretty func not available");

void initialise_syscalls() {
	set_handler(0, HlKernelMessage, 1);
	set_handler(1, HlCreateNewHandle, 0);
	set_handler(2, HlDestroyHandle, 1);
	set_handler(3, HlOpenFile, 3);
	set_handler(4, HlCloseFile, 1);
	set_handler(5, HlWriteFile, 3);
	set_handler(6, HlReadFile, 3);
	set_handler(7, HlPositionedWriteFile, 4);
	set_handler(8, HlPositionedReadFile, 4);
	set_handler(9, HlSyncFile, 1);
	set_handler(10, HlOpenDirectory, 3);
	set_handler(11, HlCloseDirectory, 1);
	set_handler(12, HlMakeDirectory, 2);
	set_handler(13, HlRemoveDirectory, 2);
	set_handler(14, HlListDirectory, 2);
	set_handler(15, HlResetDirectoryReadOffset, 1);
	set_handler(16, HlMemoryPoolAllocate, 1);
	set_handler(17, HlMemoryPoolFree, 1);
	set_handler(18, HlMemoryAllocatePool, 1);
	set_handler(19, HlMemoryFreePool, 1);
	set_handler(20, HlMemoryAllocateAligned, 1);
	set_handler(21, HlMemoryFreeAligned, 2);
	set_handler(22, HlMemorySetAttributes, 3);
	set_handler(23, HlCreateNewProcess, 0);
	set_handler(24, HlKillProcess, 1);
	set_handler(25, HlTerminateProcess, 1);
	set_handler(26, HlLoadElf, 1);
	set_handler(27, HlExit, 1);
	set_handler(28, HlOpenConsole, 2);
	set_handler(29, HlWaitForInputConsole, 0);
	set_handler(30, HlReadConsole, 2);
	set_handler(31, HlWriteConsole, 2);
}

