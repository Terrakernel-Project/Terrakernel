#ifndef SYSCALLS_H
#define SYSCALLS_H 1

#include <stdint.h>
#include <stddef.h>

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

struct Handle_Struc; // Handle_Struc is opaque to make it backwards compatible, all data access should be done via syscalls
typedef struct Handle_Struc Handle;

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR 0x0002
#define O_CREAT 0x0040
#define O_EXCL 0x0080
#define O_TRUNC 0x0200
#define O_APPEND 0x0400
#define O_DIRECTORY 0x10000

#define FLAG_FILE_LOAD_MEMORY    (1 << 31)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct {
    void     *BaseAddress;
    uint64_t  Width;
	uint64_t  Height;
	uint64_t  Pitch;
    uint64_t  BitsPerPixel;
    uint64_t  Stride;
} HlFb;

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

static inline uint64_t HlStatFileSize(Handle* hptr) {
    return syscall1(5, (uint64_t)hptr);
}

static inline int64_t HlSeekFile(Handle* hptr, int64_t offset, int whence) {
    return syscall3(6, (uint64_t)hptr, offset, whence);
}

static inline int64_t HlWriteFile(Handle* hptr, const void* __restrict dat, size_t count) {
	return syscall3(7, (uint64_t)hptr, (uint64_t)dat, count);
}

static inline int64_t HlReadFile(Handle* hptr, void* __restrict buf, size_t count) {
	return syscall3(8, (uint64_t)hptr, (uint64_t)buf, count);
}

static inline int64_t HlPositionedWriteFile(
	Handle* hptr,
	size_t offset,
	const void* __restrict dat,
	size_t count
) {
	return syscall4(9, (uint64_t)hptr, offset, (uint64_t)dat, count);
}

static inline int64_t HlPositionedReadFile(
	Handle* hptr,
	size_t offset,
	void* __restrict buf,
	size_t count
) {
	return syscall4(10, (uint64_t)hptr, offset, (uint64_t)buf, count);
}

static inline void HlSyncFile(Handle* hptr) {
	syscall1(11, (uint64_t)hptr);
}

static inline void HlOpenDirectory(Handle* hptr, const char* __restrict path, uint32_t OpenFlags) {
	syscall3(12, (uint64_t)hptr, (uint64_t)path, OpenFlags);
}

static inline void HlCloseDirectory(Handle* hptr) {
	syscall1(13, (uint64_t)hptr);
}

static inline void HlMakeDirectory(Handle* hptr, const char* __restrict path) {
	syscall2(14, (uint64_t)hptr, (uint64_t)path);
}

static inline void HlRemoveDirectory(Handle* hptr, const char* __restrict path) {
	syscall2(15, (uint64_t)hptr, (uint64_t)path);
}

static inline void HlListDirectory(Handle* hptr, void* __restrict buf) {
	syscall2(16, (uint64_t)hptr, (uint64_t)buf);
}

static inline void HlResetDirectoryReadOffset(Handle* hptr) {
	syscall1(17, (uint64_t)hptr);
}

static inline void* HlMemoryPoolAllocate(size_t n) {
	return (void*)syscall1(18, n);
}

static inline void HlMemoryPoolFree(void* ptr) {
	syscall1(19, (uint64_t)ptr);
}

static inline void* HlMemoryAllocatePool(size_t nbytes) {
	return (void*)syscall1(20, nbytes);
}

static inline void HlMemoryFreePool(void* poolptr) {
	syscall1(21, (uint64_t)poolptr);
}

static inline void* HlMemoryAllocateAligned(size_t npages) {
	return (void*)syscall1(22, npages);
}

static inline void HlMemoryFreeAligned(void* ptr, size_t npages) {
	syscall2(23, (uint64_t)ptr, npages);
}

static inline void HlMemorySetAttributes(void* ptr, size_t npages, uint64_t attributes) {
	syscall3(24, (uint64_t)ptr, npages, attributes);
}

static inline int64_t HlCreateNewProcess() {
	return syscall0(25);
}

static inline void HlKillProcess(int64_t pid) {
	syscall1(26, pid);
}

static inline void HlTerminateProcess(int64_t pid) {
	syscall1(27, pid);
}

int64_t HlExec(const char* __restrict path) {
	return syscall1(28, (uint64_t)path);
}

static inline void HlExit(int exit_code) {
	syscall1(29, exit_code);
}

static inline void HlOpenConsole(Handle* portR, Handle* portW) {
	syscall2(30, (uint64_t)portR, (uint64_t)portW);
}

static inline void HlWaitForInputConsole(Handle* portR) {
	syscall1(31, (uint64_t)portR);
}

static inline int64_t HlReadConsole(Handle* portR, void* __restrict buf, size_t count) {
	return syscall3(32, (uint64_t)portR, (uint64_t)buf, count);
}

static inline int64_t HlWriteConsole(Handle* portW, const void* __restrict dat, size_t count) {
	return syscall3(33, (uint64_t)portW, (uint64_t)dat, count);
}

static inline void HlObtainFramebuffer(Handle* hptr) {
    syscall1(34, (uint64_t)hptr);
}

static inline void HlStatFramebuffer(Handle* hptr, HlFb* buf) {
    syscall2(35, (uint64_t)hptr, (uint64_t)buf);
}

static inline void* HlRetrieveFileMappedMemory(Handle* hptr) {
    return (void*)syscall1(36, (uint64_t)hptr);
}

static inline uint64_t HlRetrieveMappedFileSize(Handle* hptr) {
    return syscall1(37, (uint64_t)hptr);
}

static inline void HlStatHandleType_Temp(Handle* hptr) {
	return syscall1(38, (uint64_t)hptr);
}

#endif
