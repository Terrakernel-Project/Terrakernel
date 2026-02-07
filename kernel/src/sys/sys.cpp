#include <arch/x86_64/syscall/handlers.hpp>
#include "syscalls.hpp"
#include <cstdio>

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

void HlStatHandleType_Temp(Handle* hptr) {
	printf("hptr->HandleType=%d\n\r", hptr->HandleType);
}

void initialise_syscalls() {
	/* HlApi 1.0 */
	set_handler(0, HlKernelMessage, 1);
	set_handler(1, HlCreateNewHandle, 0);
	set_handler(2, HlDestroyHandle, 1);
	set_handler(3, HlOpenFile, 3);
	set_handler(4, HlCloseFile, 1);
	/* HlApi 1.2 insert */
	set_handler(5, HlStatFileSize, 1);
	set_handler(6, HlSeekFile, 3);
	/* HlApi 1.0 */
	set_handler(7, HlWriteFile, 3);
	set_handler(8, HlReadFile, 3);
	set_handler(9, HlPositionedWriteFile, 4);
	set_handler(10, HlPositionedReadFile, 4);
	set_handler(11, HlSyncFile, 1);
	set_handler(12, HlOpenDirectory, 3);
	set_handler(13, HlCloseDirectory, 1);
	set_handler(14, HlMakeDirectory, 2);
	set_handler(15, HlRemoveDirectory, 2);
	set_handler(16, HlListDirectory, 2);
	set_handler(17, HlResetDirectoryReadOffset, 1);
	set_handler(18, HlMemoryPoolAllocate, 1);
	set_handler(19, HlMemoryPoolFree, 1);
	set_handler(20, HlMemoryAllocatePool, 1);
	set_handler(21, HlMemoryFreePool, 1);
	set_handler(22, HlMemoryAllocateAligned, 1);
	set_handler(23, HlMemoryFreeAligned, 2);
	set_handler(24, HlMemorySetAttributes, 3);
	set_handler(25, HlCreateNewProcess, 0);
	set_handler(26, HlKillProcess, 1);
	set_handler(27, HlTerminateProcess, 1);
	set_handler(28, HlExec, 1);
	set_handler(29, HlExit, 1);
	set_handler(30, HlOpenConsole, 2);
	set_handler(31, HlWaitForInputConsole, 1);
	set_handler(32, HlReadConsole, 3);
	set_handler(33, HlWriteConsole, 3);
	/* HlApi 1.1 */
	set_handler(34, HlObtainFramebuffer, 1);
	set_handler(35, HlStatFramebuffer, 2);
	/* HlApi 1.2 */
	set_handler(36, HlRetrieveFileMappedMemory, 1);
	set_handler(37, HlRetrieveMappedFileSize, 1);
}

