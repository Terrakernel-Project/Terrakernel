#include <ObjectManager/ObjectManager.hpp>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <ramfs/ramfs.hpp>
#include <mem/mem.hpp>
#include <drivers/tty/ldisc/ldisc.hpp>
#include <panic.hpp>
#include <exec/elf.hpp>

int64_t curr_pid = 0;

#define VALID_HNDL(hptr, type, DOCODE)                 \
do {                                                   \
    if ((hptr) == NULL ||                              \
        !ObjMan::ValidateID((hptr)->ObjectID)) {       \
        DOCODE;                                       \
    }                                                  \
} while (0);

void HlKernelMessage(const char* __restrict dat) {
    size_t len = strlen(dat);
    if (len == 0) return;

    if (dat[len - 1] == '\n')
        printf("%s\r", dat);
    else
        printf("%s\n\r", dat);
}

Handle* HlCreateNewHandle() {
    return ObjMan::CreateNewHandle();
}

void HlDestroyHandle(Handle* hptr) {
    ObjMan::DestroyHandle(hptr);
}

void HlOpenFile(Handle* hptr, const char* __restrict path, uint32_t OpenFlags) {
    if (hptr->HandleType == HANDLE_TYPE_GENERIC) return; // cannot replace 
    hptr->HandleType = HANDLE_TYPE_FILEIO;
    hptr->Flags = OpenFlags;
    hptr->RefCount = 1; // because HlOpenFile references this handle
    hptr->OwnerPID = 0;
    hptr->LastError = 0;

    stat s;
    ramfs::stat(path, &s);

    hptr->Payload.File.FileDescriptor = ramfs::open(path, OpenFlags);
    hptr->Payload.File.FileOffset = 0;
    hptr->Payload.File.FileSize = s.st_size;

    if (OpenFlags & FLAG_FILE_LOAD_MEMORY) {
        size_t npages = (s.st_size + 0xFFF) / 0x1000;
        hptr->Payload.File.MappedAddress = mem::usr::alloc(npages);
        hptr->Payload.File.MappedSize = npages;
    }

    hptr->Payload.File.OpenFlags = OpenFlags;
    hptr->Payload.File.FileSystemID = 0; // TODO make it work

    ramfs::lseek(hptr->Payload.File.FileDescriptor, 0, SEEK_SET);
}

void HlCloseFile(Handle* hptr) {
    VALID_HNDL(hptr, HANDLE_TYPE_FILEIO, return)
    ramfs::close(hptr->Payload.File.FileDescriptor);
    
    if (hptr->Payload.File.OpenFlags & FLAG_FILE_LOAD_MEMORY) {
        mem::usr::free(
            hptr->Payload.File.MappedAddress,
            hptr->Payload.File.MappedSize
        );
    }

    hptr->HandleType = HANDLE_TYPE_GENERIC;
}

int64_t HlWriteFile(Handle* hptr, const void* __restrict dat, size_t count) {
    VALID_HNDL(hptr, HANDLE_TYPE_FILEIO, return -1)

    if (hptr->Payload.File.OpenFlags & FLAG_FILE_LOAD_MEMORY) {
        size_t off = hptr->Payload.File.FileOffset;
        size_t max = hptr->Payload.File.FileSize;
        if (off >= max) return 0;

        size_t to_copy = count;
        if (off + to_copy > max)
            to_copy = max - off;

        mem::memcpy(
            (uint8_t*)hptr->Payload.File.MappedAddress + off,
            dat,
            to_copy
        );
        hptr->Payload.File.FileOffset += to_copy;
        return to_copy;
    }

    int64_t r = ramfs::write(
        hptr->Payload.File.FileDescriptor,
        dat,
        count
    );
    if (r > 0) hptr->Payload.File.FileOffset += r;
    return r;
}

int64_t HlReadFile(Handle* hptr, void* __restrict buf, size_t count) {
    VALID_HNDL(hptr, HANDLE_TYPE_FILEIO, return -1)

    if (hptr->Payload.File.OpenFlags & FLAG_FILE_LOAD_MEMORY) {
        size_t off = hptr->Payload.File.FileOffset;
        size_t max = hptr->Payload.File.FileSize;
        if (off >= max) return 0;

        size_t to_copy = count;
        if (off + to_copy > max)
            to_copy = max - off;

        mem::memcpy(
            buf,
            (uint8_t*)hptr->Payload.File.MappedAddress + off,
            to_copy
        );
        hptr->Payload.File.FileOffset += to_copy;
        return to_copy;
    }

    int64_t r = ramfs::read(
        hptr->Payload.File.FileDescriptor,
        buf,
        count
    );
    if (r > 0) hptr->Payload.File.FileOffset += r;
    return r;
}

int64_t HlPositionedWriteFile(
    Handle* hptr,
    size_t offset,
    const void* __restrict dat,
    size_t count
) {
    VALID_HNDL(hptr, HANDLE_TYPE_FILEIO, return -1)

    if (hptr->Payload.File.OpenFlags & FLAG_FILE_LOAD_MEMORY) {
        size_t max = hptr->Payload.File.FileSize;
        if (offset >= max) return 0;

        size_t to_copy = count;
        if (offset + to_copy > max)
            to_copy = max - offset;

        mem::memcpy(
            (uint8_t*)hptr->Payload.File.MappedAddress + offset,
            dat,
            to_copy
        );
        return to_copy;
    }

    int64_t off = ramfs::tell(hptr->Payload.File.FileDescriptor);
    ramfs::lseek(hptr->Payload.File.FileDescriptor, offset, SEEK_SET);
    int64_t r = ramfs::write(
        hptr->Payload.File.FileDescriptor,
        dat,
        count
    );
    ramfs::lseek(hptr->Payload.File.FileDescriptor, off, SEEK_SET);
    return r;
}

int64_t HlPositionedReadFile(
    Handle* hptr,
    size_t offset,
    void* __restrict buf,
    size_t count
) {
    VALID_HNDL(hptr, HANDLE_TYPE_FILEIO, return -1)

    if (hptr->Payload.File.OpenFlags & FLAG_FILE_LOAD_MEMORY) {
        size_t max = hptr->Payload.File.FileSize;
        if (offset >= max) return 0;

        size_t to_copy = count;
        if (offset + to_copy > max)
            to_copy = max - offset;

        mem::memcpy(
            buf,
            (uint8_t*)hptr->Payload.File.MappedAddress + offset,
            to_copy
        );
        return to_copy;
    }

    int64_t off = ramfs::tell(hptr->Payload.File.FileDescriptor);
    ramfs::lseek(hptr->Payload.File.FileDescriptor, offset, SEEK_SET);
    int64_t r = ramfs::read(
        hptr->Payload.File.FileDescriptor,
        buf,
        count
    );
    ramfs::lseek(hptr->Payload.File.FileDescriptor, off, SEEK_SET);
    return r;
}

void HlSyncFile(Handle* hptr) {
    VALID_HNDL(hptr, HANDLE_TYPE_FILEIO, return)

    if (hptr->Payload.File.OpenFlags & FLAG_FILE_LOAD_MEMORY) {
        int64_t off = ramfs::tell(hptr->Payload.File.FileDescriptor);
        ramfs::lseek(hptr->Payload.File.FileDescriptor, 0, SEEK_SET);
        ramfs::write(
            hptr->Payload.File.FileDescriptor,
            hptr->Payload.File.MappedAddress,
            hptr->Payload.File.FileSize
        );
        ramfs::lseek(hptr->Payload.File.FileDescriptor, off, SEEK_SET);
    }
}

void HlOpenDirectory(Handle* hptr, const char* __restrict path, uint32_t OpenFlags) {
    (void)OpenFlags;

    VALID_HNDL(hptr, HANDLE_TYPE_GENERIC, return)

    DIR* dir = ramfs::opendir(path);
    if (!dir) {
        hptr->LastError = -1;
        return;
    }

    hptr->HandleType = HANDLE_TYPE_DIRIO;
    hptr->Flags = 0;
    hptr->RefCount = 1;
    hptr->OwnerPID = 0;
    hptr->LastError = 0;

    hptr->Payload.Directory.DirDescriptor = (int64_t)dir;
    hptr->Payload.Directory.ReadOffset = 0;
    hptr->Payload.Directory.EntryCountCache = 0;
}

void HlCloseDirectory(Handle* hptr) {
    VALID_HNDL(hptr, HANDLE_TYPE_DIRIO, return)

    DIR* dir = (DIR*)hptr->Payload.Directory.DirDescriptor;
    if (dir)
        ramfs::closedir(dir);

    hptr->Payload.Directory.DirDescriptor = 0;
    hptr->HandleType = HANDLE_TYPE_GENERIC;
}

void HlMakeDirectory(Handle* hptr, const char* __restrict path) {
    (void)hptr;
    ramfs::mkdir(path, 0777);
}

void HlRemoveDirectory(Handle* hptr, const char* __restrict path) {
    (void)hptr;
    ramfs::rmdir(path);
}

// buf must be 1024 bytes, first 8 bytes are number of entries, followed by a 2D array of strings
void HlListDirectory(Handle* hptr, void* __restrict buf) {
    VALID_HNDL(hptr, HANDLE_TYPE_DIRIO, return)

    DIR* dir = (DIR*)hptr->Payload.Directory.DirDescriptor;
    if (!dir)
        return;

    uint64_t* count = (uint64_t*)buf;
    char* base = (char*)buf + sizeof(uint64_t);

    *count = 0;

    for (size_t i = 0; i < 64; ++i) {
        dirent* ent = ramfs::readdir(dir);
        if (!ent)
            break;

        char* dst = base + i * (NAME_MAX + 1);
        strncpy(dst, ent->d_name, NAME_MAX);
        dst[NAME_MAX] = '\0';

        (*count)++;
        hptr->Payload.Directory.ReadOffset++;
    }
}

void HlResetDirectoryReadOffset(Handle* hptr) {
    VALID_HNDL(hptr, HANDLE_TYPE_DIRIO, return)

    DIR* dir = (DIR*)hptr->Payload.Directory.DirDescriptor;
    if (!dir)
        return;

    ramfs::closedir(dir);
    hptr->Payload.Directory.DirDescriptor = 0;
    hptr->Payload.Directory.ReadOffset = 0;
    hptr->LastError = -1; // caller must reopen
}

void* HlMemoryPoolAllocate(size_t n) {
    return mem::usr::alloc((n + 0xFFF) / 0x1000);
}

void HlMemoryPoolFree(void* ptr) {
    mem::usr::free(ptr, 1);
}

struct Pool {
    void* pool_base; // kernel prespective, this is void*, but app prespective, it is const void*
    size_t nbytes;
    size_t npages;
};

void* HlMemoryAllocatePool(size_t nbytes) {
    size_t meta_pages = (sizeof(Pool) + 0xFFF) / 0x1000;
    Pool* newpool = (Pool*)mem::usr::alloc(meta_pages);

    newpool->npages = (nbytes + 0xFFF) / 0x1000;
    newpool->pool_base = mem::usr::alloc(newpool->npages);
    newpool->nbytes = nbytes;

    return (void*)newpool;
}

void HlMemoryFreePool(void* poolptr) {
    Pool* pool = (Pool*)poolptr;
    mem::usr::free(pool->pool_base, pool->npages);
    mem::usr::free(pool, (sizeof(Pool) + 0xFFF) / 0x1000);
}

void* HlMemoryAllocateAligned(size_t npages) {
    return mem::usr::alloc(npages);
}

void HlMemoryFreeAligned(void* ptr, size_t npages) {
    mem::usr::free(ptr, npages);
}

void HlMemorySetAttributes(void* ptr, size_t npages, uint64_t attributes) {
    mem::vmm::mmap(ptr, ptr, npages, attributes | PAGE_USER | PAGE_PRESENT);
}

// returns the process ID
int64_t HlCreateNewProcess() {
    return -1; // NO-OP
}

void HlKillProcess(int64_t pid) {
    (void)pid;
}

void HlTerminateProcess(int64_t pid) {
    (void)pid;
}

void HlLoadElf(const void* __restrict datbase) {
    run_elf((void*)datbase, 0, true);
}

void HlExit(int exit_code) {
    (void)exit_code;
    if (curr_pid == 0) {
        Log::panic("kernel init process attempted to call HlExit()...");
        panic("no processes remaining... halting...");
    }
}

void HlOpenConsole(Handle* portR, Handle* portW) {
    portR->HandleType = HANDLE_TYPE_CONSOLE_R;
    portR->Flags = 0;
    portR->RefCount = 1;
    portR->OwnerPID = 0;
    portR->LastError = 0;

    portW->HandleType = HANDLE_TYPE_CONSOLE_W;
    portW->Flags = 0;
    portW->RefCount = 1;
    portW->OwnerPID = 0;
    portW->LastError = 0;
}

void HlWaitForInputConsole() {
    while (!drivers::tty::ldisc::has_input());
}

int64_t HlReadConsole(void* __restrict buf, size_t count) {
    return drivers::tty::ldisc::read(false, (char*)buf, count);
}

int64_t HlWriteConsole(const void* __restrict dat, size_t count) {
    return drivers::tty::ldisc::write((const char*)dat, count);
}
