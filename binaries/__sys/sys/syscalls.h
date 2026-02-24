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

typedef struct {
    const void* PoolBase;
    size_t		NBytes;
    size_t		NPages;
} HlPool;

#define SYSARG(arg) ((uint64_t)arg);

void HlKernelMessage(const char* __restrict dat) {
	syscall1(0, SYSARG(dat));
}

Handle* HlCreateNewHandle() {
	return (Handle*)syscall0(1);
}

void HlDestroyHandle(Handle* hptr) {
	
}

void HlOpenFile(Handle* hptr, const char* __restrict path, uint32_t OpenFlags) {
	
}

void HlCloseFile(Handle* hptr) {
	
}

uint64_t HlStatFileSize(Handle* hptr) {
	
}

int64_t HlSeekFile(Handle* hptr, int64_t offset, int whence) {
	
}

int64_t HlWriteFile(Handle* hptr, const void* __restrict dat, size_t count) {
	
}

int64_t HlReadFile(Handle* hptr, void* __restrict buf, size_t count) {
	
}

int64_t HlPositionedWriteFile(Handle* hptr, size_t offset, const void* __restrict dat, size_t count) {
	
}

int64_t HlPositionedReadFile(Handle* hptr, size_t offset, void* __restrict buf, size_t count) {
	
}

void HlSyncFile(Handle* hptr) {
	
}

void HlOpenDirectory(Handle* hptr, const char* __restrict path, uint32_t OpenFlags) {
	
}

void HlCloseDirectory(Handle* hptr) {
	
}

void HlMakeDirectory(const char* __restrict path) {
	
}

void HlRemoveDirectory(const char* __restrict path) {
	
}

void HlListDirectory(Handle* hptr, void* __restrict buf) {
	
}

void HlResetDirectoryReadOffset(Handle* hptr) {
	
}

void* HlMemoryPoolAllocate(size_t n) {
	
}

void HlMemoryPoolFree(void* ptr) {
	
}

void* HlMemoryAllocatePool(size_t nbytes) {
	
}

void HlMemoryFreePool(void* poolptr) {
	
}

void* HlMemoryAllocateAligned(size_t npages) {

}

void HlMemoryFreeAligned(void* ptr, size_t npages) {
	
}

void HlMemorySetAttributes(void* ptr, size_t npages, uint64_t attributes) {
	
}

int64_t HlCreateNewProcess() {
	
}

void HlKillProcess(int64_t pid) {
	
}

void HlTerminateProcess(int64_t pid) {
	
}

int HlExec(const char* __restrict path, const char* args[], const char* env_vars[]) {
	
}

void HlExit(int error_code) {
	
}

void HlOpenConsole(Handle* portR, Handle* portW) {
	
}

void HlWaitForInputConsole(Handle* portR) {
	
}

int64_t HlReadConsole(Handle* portW, void* __restrict buf, size_t count) {
	
}

int64_t HlWriteConsole(Handle* portW, const void* __restrict dat, size_t count) {
	
}

void HlObtainFramebuffer(Handle* hptr) {
	
}

void HlStatFramebuffer(Handle* hptr, HlFb* buf) {
	
}

void* HlRetrieveFileMappedMemory(Handle* hptr) {
	
}

uint64_t HlRetrieveMappedFileSize(Handle* hptr) {
	
}

#endif
