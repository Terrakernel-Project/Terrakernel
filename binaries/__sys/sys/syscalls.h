#ifndef SYSCALLS_H
#define SYSCALLS_H 1

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define NAME_MAX 255
#define PATH_MAX 4096
#define SYSARG(arg) ((uint64_t)arg)

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

typedef struct {
    const void* PoolBase;
    size_t		NBytes;
    size_t		NPages;
} HlPool;

typedef struct {
    uint32_t width_cells;
    uint32_t height_cells;

    uint32_t width_pixels;
    uint32_t height_pixels;

    uint32_t cell_width_pixels;
    uint32_t cell_height_pixels;

    bool supports_colour;
    bool supports_unicode;
    bool supports_mouse;
    bool supports_resize;

    uint32_t max_width_cells;
    uint32_t max_height_cells;
} HlConsoleStat;

/* Syscall table:
 *  0  HlKernelMessage
 *  1  HlCreateNewHandle
 *  2  HlDestroyHandle
 *  3  HlOpenFile
 *  4  HlCloseFile
 *  5  HlStatFileSize
 *  6  HlSeekFile
 *  7  HlWriteFile
 *  8  HlReadFile
 *  9  HlPositionedWriteFile
 * 10  HlPositionedReadFile
 * 11  HlSyncFile
 * 12  HlOpenDirectory
 * 13  HlCloseDirectory
 * 14  HlMakeDirectory
 * 15  HlRemoveDirectory
 * 16  HlListDirectory
 * 17  HlResetDirectoryReadOffset
 * 18  HlMemoryNewPool
 * 19  HlMemoryDestroyPool
 * 20  HlMemoryAllocatePool
 * 21  HlMemoryFreePool
 * 22  HlMemoryAllocateAligned
 * 23  HlMemoryFreeAligned
 * 24  HlMemorySetAttributes
 * 25  HlCreateNewProcess
 * 26  HlKillProcess
 * 27  HlTerminateProcess
 * 28  HlExec
 * 29  HlExit
 * 30  HlOpenConsole
 * 31  HlWaitForInputConsole
 * 32  HlReadConsole
 * 33  HlWriteConsole
 * 34  HlStatConsole
 * 35  HlObtainFramebuffer
 * 36  HlStatFramebuffer
 * 37  HlRetrieveFileMappedMemory
 * 38  HlRetrieveMappedFileSize
 * 39  HlSleepMs
 */

static inline void HlKernelMessage(const char* __restrict dat) {
	syscall1(0, SYSARG(dat));
}

static inline Handle* HlCreateNewHandle() {
	return (Handle*)syscall0(1);
}

static inline void HlDestroyHandle(Handle* hptr) {
	syscall1(2, SYSARG(hptr));
}

static inline void HlOpenFile(Handle* hptr, const char* __restrict path, uint32_t OpenFlags) {
	syscall3(3, SYSARG(hptr), SYSARG(path), SYSARG(OpenFlags));
}

static inline void HlCloseFile(Handle* hptr) {
	syscall1(4, SYSARG(hptr));
}

static inline uint64_t HlStatFileSize(Handle* hptr) {
	return syscall1(5, SYSARG(hptr));
}

static inline int64_t HlSeekFile(Handle* hptr, int64_t offset, int whence) {
	return (int64_t)syscall3(6, SYSARG(hptr), SYSARG(offset), SYSARG(whence));
}

static inline int64_t HlWriteFile(Handle* hptr, const void* __restrict dat, size_t count) {
	return (int64_t)syscall3(7, SYSARG(hptr), SYSARG(dat), SYSARG(count));
}

static inline int64_t HlReadFile(Handle* hptr, void* __restrict buf, size_t count) {
	return (int64_t)syscall3(8, SYSARG(hptr), SYSARG(buf), SYSARG(count));
}

static inline int64_t HlPositionedWriteFile(Handle* hptr, size_t offset, const void* __restrict dat, size_t count) {
	return (int64_t)syscall4(9, SYSARG(hptr), SYSARG(offset), SYSARG(dat), SYSARG(count));
}

static inline int64_t HlPositionedReadFile(Handle* hptr, size_t offset, void* __restrict buf, size_t count) {
	return (int64_t)syscall4(10, SYSARG(hptr), SYSARG(offset), SYSARG(buf), SYSARG(count));
}

static inline void HlSyncFile(Handle* hptr) {
	syscall1(11, SYSARG(hptr));
}

static inline void HlOpenDirectory(Handle* hptr, const char* __restrict path, uint32_t OpenFlags) {
	syscall3(12, SYSARG(hptr), SYSARG(path), SYSARG(OpenFlags));
}

static inline void HlCloseDirectory(Handle* hptr) {
	syscall1(13, SYSARG(hptr));
}

static inline void HlMakeDirectory(const char* __restrict path) {
	syscall1(14, SYSARG(path));
}

static inline void HlRemoveDirectory(const char* __restrict path) {
	syscall1(15, SYSARG(path));
}

static inline void HlListDirectory(Handle* hptr, void* __restrict buf) {
	syscall2(16, SYSARG(hptr), SYSARG(buf));
}

static inline void HlResetDirectoryReadOffset(Handle* hptr) {
	syscall1(17, SYSARG(hptr));
}

static inline void* HlMemoryPoolAllocate(size_t n) {
	return (void*)syscall1(18, SYSARG(n));
}

static inline void HlMemoryPoolFree(void* ptr) {
	syscall1(19, SYSARG(ptr));
}

static inline HlPool* HlMemoryNewPool(size_t nbytes) {
	return (HlPool*)syscall1(20, SYSARG(nbytes));
}

static inline void HlMemoryDestroyPool(HlPool* poolptr) {
	syscall1(21, SYSARG(poolptr));
}

static inline void* HlMemoryAllocateAligned(size_t npages) {
	return (void*)syscall1(22, SYSARG(npages));
}

static inline void HlMemoryFreeAligned(void* ptr, size_t npages) {
	syscall2(23, SYSARG(ptr), SYSARG(npages));
}

static inline void HlMemorySetAttributes(void* ptr, size_t npages, uint64_t attributes) {
	syscall3(24, SYSARG(ptr), SYSARG(npages), SYSARG(attributes));
}

static inline int64_t HlCreateNewProcess() {
	return (int64_t)syscall0(25);
}

static inline void HlKillProcess(int64_t pid) {
	syscall1(26, SYSARG(pid));
}

static inline void HlTerminateProcess(int64_t pid) {
	syscall1(27, SYSARG(pid));
}

static inline int HlExec(const char* __restrict path, const char* args[], const char* env_vars[]) {
	return (int)syscall3(28, SYSARG(path), SYSARG(args), SYSARG(env_vars));
}

static inline void HlExit(int error_code) {
	syscall1(29, SYSARG(error_code));
}

static inline void HlOpenConsole(Handle* portR, Handle* portW) {
	syscall2(30, SYSARG(portR), SYSARG(portW));
}

static inline void HlWaitForInputConsole(Handle* portR) {
	syscall1(31, SYSARG(portR));
}

static inline int64_t HlReadConsole(Handle* portW, void* __restrict buf, size_t count) {
	return (int64_t)syscall3(32, SYSARG(portW), SYSARG(buf), SYSARG(count));
}

static inline int64_t HlWriteConsole(Handle* portW, const void* __restrict dat, size_t count) {
	return (int64_t)syscall3(33, SYSARG(portW), SYSARG(dat), SYSARG(count));
}

static inline void HlStatConsole(Handle* anyport, HlConsoleStat* stat) {
	syscall2(34, SYSARG(anyport), SYSARG(stat));
}

static inline void HlObtainFramebuffer(Handle* hptr) {
	syscall1(35, SYSARG(hptr));
}

static inline void HlStatFramebuffer(Handle* hptr, HlFb* buf) {
	syscall2(36, SYSARG(hptr), SYSARG(buf));
}

static inline void* HlRetrieveFileMappedMemory(Handle* hptr) {
	return (void*)syscall1(37, SYSARG(hptr));
}

static inline uint64_t HlRetrieveMappedFileSize(Handle* hptr) {
	return syscall1(38, SYSARG(hptr));
}

static inline void HlSleepMs(uint64_t ms) {
	syscall1(39, SYSARG(ms));
}

#endif
