#ifndef SYSCALLS_H
#define SYSCALLS_H 1

#define NAME_MAX 255
#define PATH_MAX 4096

static inline uint64_t syscall0(
	uint64_t n
) {
    uint64_t ret;
    asm volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall1(
	uint64_t n, uint64_t a1
) {
    uint64_t ret;
    asm volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall2(
	uint64_t n, uint64_t a1, uint64_t a2
) {
    uint64_t ret;
    asm volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall3(
	uint64_t n, uint64_t a1, uint64_t a2,
	uint64_t a3
) {
    uint64_t ret;
    asm volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall4(
    uint64_t n, uint64_t a1, uint64_t a2,
    uint64_t a3, uint64_t a4
) {
    uint64_t ret;
    register uint64_t r10 asm("r10") = a4;

    asm volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall5(
    uint64_t n, uint64_t a1, uint64_t a2,
    uint64_t a3, uint64_t a4, uint64_t a5
) {
    uint64_t ret;
    register uint64_t r10 asm("r10") = a4;
    register uint64_t r8  asm("r8")  = a5;

    asm volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3),
          "r"(r10), "r"(r8)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall6(
    uint64_t n, uint64_t a1, uint64_t a2,
    uint64_t a3, uint64_t a4, uint64_t a5,
    uint64_t a6
) {
    uint64_t ret;
    register uint64_t r10 asm("r10") = a4;
    register uint64_t r8  asm("r8")  = a5;
    register uint64_t r9  asm("r9")  = a6;

    asm volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3),
          "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}

struct Handle_Struc;
typedef struct Handle_Struc Handle;

static inline void HlKernelMessage(const char* __restrict dat) {
	syscall1(0, (uint64_t)dat);
}

static inline Handle* HlCreateNewHandle() {
	return (Handle*)syscall0(1);
}

static inline void HlDestroyHandle(Handle* hptr) {
	syscall1(2, (uint64_t)hptr);
}

static inline void HlOpenFile(Handle* hptr, const char* __restrict path, uint32_t OpenFlags) {
	syscall3(3, (uint64_t)hptr, (uint64_t)path, OpenFlags);
}

static inline void HlCloseFile(Handle* hptr) {
	syscall1(4, (uint64_t)hptr);
}

static inline int64_t HlWriteFile(Handle* hptr, const void* __restrict dat, size_t count) {
	return syscall3(5, (uint64_t)hptr, (uint64_t)dat, count);
}

static inline int64_t HlReadFile(Handle* hptr, void* __restrict buf, size_t count) {
	return syscall3(6, (uint64_t)hptr, (uint64_t)buf, count);
}

static inline int64_t HlPositionedWriteFile(
	Handle* hptr,
	size_t offset,
	const void* __restrict dat,
	size_t count
) {
	return syscall4(7, (uint64_t)hptr, offset, (uint64_t)dat, count);
}

static inline int64_t HlPositionedReadFile(
	Handle* hptr,
	size_t offset,
	void* __restrict buf,
	size_t count
) {
	return syscall4(8, (uint64_t)hptr, offset, (uint64_t)buf, count);
}

static inline void HlSyncFile(Handle* hptr) {
	syscall1(9, (uint64_t)hptr);
}

static inline void HlOpenDirectory(Handle* hptr, const char* __restrict path, uint32_t OpenFlags) {
	syscall3(10, (uint64_t)hptr, (uint64_t)path, OpenFlags);
}

static inline void HlCloseDirectory(Handle* hptr) {
	syscall1(11, (uint64_t)hptr);
}

static inline void HlMakeDirectory(Handle* hptr, const char* __restrict path) {
	syscall2(12, (uint64_t)hptr, (uint64_t)path);
}

static inline void HlRemoveDirectory(Handle* hptr, const char* __restrict path) {
	syscall2(13, (uint64_t)hptr, (uint64_t)path);
}

static inline void HlListDirectory(Handle* hptr, void* __restrict buf) {
	syscall2(14, (uint64_t)hptr, (uint64_t)buf);
}

static inline void HlResetDirectoryReadOffset(Handle* hptr) {
	syscall1(15, (uint64_t)hptr);
}

static inline void* HlMemoryPoolAllocate(size_t n) {
	return (void*)syscall1(16, n);
}

static inline void HlMemoryPoolFree(void* ptr) {
	syscall1(17, (uint64_t)ptr);
}

static inline void* HlMemoryAllocatePool(size_t nbytes) {
	return (void*)syscall1(18, nbytes);
}

static inline void HlMemoryFreePool(void* poolptr) {
	syscall1(19, (uint64_t)poolptr);
}

static inline void* HlMemoryAllocateAligned(size_t npages) {
	return (void*)syscall1(20, npages);
}

static inline void HlMemoryFreeAligned(void* ptr, size_t npages) {
	syscall2(21, (uint64_t)ptr, npages);
}

static inline void HlMemorySetAttributes(void* ptr, size_t npages, uint64_t attributes) {
	syscall3(22, (uint64_t)ptr, npages, attributes);
}

static inline int64_t HlCreateNewProcess() {
	return syscall0(23);
}

static inline void HlKillProcess(int64_t pid) {
	syscall1(24, pid);
}

static inline void HlTerminateProcess(int64_t pid) {
	syscall1(25, pid);
}

static inline void HlLoadElf(const void* __restrict datbase) {
	syscall1(26, (uint64_t)datbase);
}

static inline void HlExit(int exit_code) {
	syscall1(27, exit_code);
}

static inline void HlOpenConsole(Handle* portR, Handle* portW) {
	syscall2(28, (uint64_t)portR, (uint64_t)portW);
}

static inline void HlWaitForInputConsole(Handle* portR) {
	syscall1(29, (uint64_t)portR);
}

static inline int64_t HlReadConsole(Handle* portR, void* __restrict buf, size_t count) {
	return syscall3(30, (uint64_t)portR, (uint64_t)buf, count);
}

static inline int64_t HlWriteConsole(Handle* portW, const void* __restrict dat, size_t count) {
	return syscall3(31, (uint64_t)portW, (uint64_t)dat, count);
}

#endif
