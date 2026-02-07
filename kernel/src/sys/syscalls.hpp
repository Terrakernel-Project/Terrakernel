#ifndef SYSCALLS_HPP
#define SYSCALLS_HPP 1

#include <ObjectManager/ObjectManager.hpp>

void HlKernelMessage(const char* __restrict dat);
Handle* HlCreateNewHandle();
void HlDestroyHandle(Handle* hptr);
void HlOpenFile(Handle* hptr, const char* __restrict path, uint32_t OpenFlags);
void HlCloseFile(Handle* hptr);
uint64_t HlStatFileSize(Handle* hptr);
int64_t HlSeekFile(Handle* hptr, int64_t offset, int whence);
int64_t HlWriteFile(Handle* hptr, const void* __restrict dat, size_t count);
int64_t HlReadFile(Handle* hptr, void* __restrict buf, size_t count);
int64_t HlPositionedWriteFile(Handle* hptr, size_t offset, const void* __restrict dat, size_t count);
int64_t HlPositionedReadFile(Handle* hptr, size_t offset, void* __restrict buf, size_t count);
void HlSyncFile(Handle* hptr);
void HlOpenDirectory(Handle* hptr, const char* __restrict path, uint32_t OpenFlags);
void HlCloseDirectory(Handle* hptr);
void HlMakeDirectory(Handle* hptr, const char* __restrict path);
void HlRemoveDirectory(Handle* hptr, const char* __restrict path);
void HlListDirectory(Handle* hptr, void* __restrict buf);
void HlResetDirectoryReadOffset(Handle* hptr);
void* HlMemoryPoolAllocate(size_t n);
void HlMemoryPoolFree(void* ptr);
void* HlMemoryAllocatePool(size_t nbytes);
void HlMemoryFreePool(void* poolptr);
void* HlMemoryAllocateAligned(size_t npages);
void HlMemoryFreeAligned(void* ptr, size_t npages);
void HlMemorySetAttributes(void* ptr, size_t npages, uint64_t attributes);
int64_t HlCreateNewProcess();
void HlKillProcess(int64_t pid);
void HlTerminateProcess(int64_t pid);
int HlExec(const char* __restrict path, const char* args[], const char* env_vars[]);
void HlExit(int error_code);
void HlOpenConsole(Handle* portR, Handle* portW);
void HlWaitForInputConsole(Handle* portR);
int64_t HlReadConsole(Handle* portW, void* __restrict buf, size_t count);
int64_t HlWriteConsole(Handle* portW, const void* __restrict dat, size_t count);
void HlObtainFramebuffer(Handle* hptr);
void HlStatFramebuffer(Handle* hptr, void* buf);
void* HlRetrieveFileMappedMemory(Handle* hptr);
uint64_t HlRetrieveMappedFileSize(Handle* hptr);

#endif
