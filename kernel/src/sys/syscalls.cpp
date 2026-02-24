#include <ObjectManager/ObjectManager.hpp>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <mem/mem.hpp>
#include <drivers/tty/ldisc/ldisc.hpp>
#include <panic.hpp>
#include <lib/Flanterm/gfx.h>
#include <config.hpp>
#include <vfs/vfs.hpp>
#include <subsystems/ramfs/ramfs.hpp>

//hptr->HandleType != type
#define VALID_HNDL(hptr, type, DOCODE)                 \
do {                                                   \
    if ((hptr) == NULL ||                              \
        !ObjMan::ValidateID((hptr)->ObjectID)          \
    ) {                                                \
        DOCODE;                                        \
    }                                                  \
} while (0);

#ifdef CONFIG_ENABLE_SYSCALL_DEBUGGING
#warning "SYSCALL DEBUGGING IS ENABLED!"
#define SDPRINTF(fmt, ...) printf("[%s] " fmt "\n", __PRETTY_FUNCTION__, ##__VA_ARGS__)
#else
#define SDPRINTF(fmt, ...)
#endif

void HlKernelMessage(const char* __restrict dat) {
	SDPRINTF("kernel message: data=%p", dat);

    size_t len = strlen(dat);
    if (len == 0) return;

    if (dat[len - 1] == '\n')
        printf("%s\r", dat);
    else
        printf("%s\n\r", dat);
}

Handle* HlCreateNewHandle() {
	SDPRINTF("creating new handle");

    Handle* h = ObjMan::CreateNewHandle();
    h->HandleType = HANDLE_TYPE_GENERIC;
    
    SDPRINTF("handle created: handle=%p, id=%lu", h, h->ObjectID);
    return h;
}

void HlDestroyHandle(Handle* hptr) {
	SDPRINTF("destroying handle: handle=%p", hptr);

    ObjMan::DestroyHandle(hptr);
}

void HlOpenFile(Handle* hptr, const char* __restrict path, uint32_t OpenFlags) {
    SDPRINTF("opening file: handle=%p, path='%s', flags=0x%x", hptr, path, OpenFlags);

    int fd = vfs::open(path, OpenFlags);
    if (fd < 0) {
        SDPRINTF("open failed: path='%s', error=%d", path, fd);
        hptr->LastError = -1;
        return;
    }

    hptr->HandleType = HANDLE_TYPE_FILEIO;
    hptr->Flags = OpenFlags;
    hptr->RefCount = 1;
    hptr->OwnerPID = 0;
    hptr->LastError = 0;

    hptr->Payload.File.FilePath = path;
    hptr->Payload.File.FileDescriptor = fd;
    hptr->Payload.File.FileOffset = 0;
    hptr->Payload.File.FileSize = vfs::stat_sz(fd);
    hptr->Payload.File.OpenFlags = OpenFlags;
    hptr->Payload.File.FileSystemID = 0;

    if (OpenFlags & FLAG_FILE_LOAD_MEMORY) {
        size_t npages = (hptr->Payload.File.FileSize + 0xFFF) / 0x1000;
        hptr->Payload.File.MappedAddress = mem::usr::alloc(npages);
        hptr->Payload.File.MappedSize = npages;
        
        SDPRINTF("loading file into memory: addr=%p, pages=%zu, size=%zu", 
                 hptr->Payload.File.MappedAddress, npages, hptr->Payload.File.FileSize);
        
        vfs::seek_file(fd, 0, SEEK_SET);
        vfs::read_file(fd, hptr->Payload.File.MappedAddress, hptr->Payload.File.FileSize);
    }

    vfs::seek_file(fd, 0, SEEK_SET);
    
    SDPRINTF("file opened successfully: fd=%d, size=%zu", fd, hptr->Payload.File.FileSize);
}

void HlCloseFile(Handle* hptr) {
    SDPRINTF("closing file: handle=%p", hptr);

    VALID_HNDL(hptr, HANDLE_TYPE_FILEIO, return)
    
    int fd = hptr->Payload.File.FileDescriptor;
    vfs::close(fd);
    
    if (hptr->Payload.File.OpenFlags & FLAG_FILE_LOAD_MEMORY) {
        SDPRINTF("freeing mapped memory: addr=%p, pages=%zu", 
                 hptr->Payload.File.MappedAddress, hptr->Payload.File.MappedSize);
        mem::usr::free(
            hptr->Payload.File.MappedAddress,
            hptr->Payload.File.MappedSize
        );
    }

    hptr->HandleType = HANDLE_TYPE_GENERIC;
    SDPRINTF("file closed: fd=%d", fd);
}

uint64_t HlStatFileSize(Handle* hptr) {
    SDPRINTF("stat file size: handle=%p", hptr);

    VALID_HNDL(hptr, HANDLE_TYPE_FILEIO, return (uint64_t)-1)

    uint64_t size = vfs::stat_sz(hptr->Payload.File.FileDescriptor);

    SDPRINTF("file size: %zu bytes", size);
    return size;
}

int64_t HlSeekFile(Handle* hptr, int64_t offset, int whence) {
    SDPRINTF("seeking file: handle=%p, offset=%ld, whence=%d", hptr, offset, whence);

    VALID_HNDL(hptr, HANDLE_TYPE_FILEIO, return -1)
    
    int64_t result = vfs::seek_file(hptr->Payload.File.FileDescriptor, offset, whence);
    if (result >= 0) {
        hptr->Payload.File.FileOffset = result;
        SDPRINTF("seek successful: new_offset=%ld", result);
    } else {
        SDPRINTF("seek failed: error=%ld", result);
    }
    return result;
}

int64_t HlWriteFile(Handle* hptr, const void* __restrict dat, size_t count) {
    SDPRINTF("writing file: handle=%p, data=%p, count=%zu", hptr, dat, count);

    VALID_HNDL(hptr, HANDLE_TYPE_FILEIO, return -1)

    if (hptr->Payload.File.OpenFlags & FLAG_FILE_LOAD_MEMORY) {
        size_t off = hptr->Payload.File.FileOffset;
        size_t max = hptr->Payload.File.FileSize;
        if (off >= max) {
            SDPRINTF("write beyond EOF: offset=%zu, filesize=%zu", off, max);
            return 0;
        }

        size_t to_copy = count;
        if (off + to_copy > max)
            to_copy = max - off;

        mem::memcpy(
            (uint8_t*)hptr->Payload.File.MappedAddress + off,
            dat,
            to_copy
        );
        hptr->Payload.File.FileOffset += to_copy;
        SDPRINTF("wrote to mapped file: bytes=%zu, new_offset=%zu", to_copy, hptr->Payload.File.FileOffset);
        return to_copy;
    }

    int64_t r = vfs::write_file(
        hptr->Payload.File.FileDescriptor,
        dat,
        count
    );
    if (r > 0) {
        hptr->Payload.File.FileOffset += r;
        SDPRINTF("write successful: bytes=%ld, new_offset=%zu", r, hptr->Payload.File.FileOffset);
    } else {
        SDPRINTF("write failed: error=%ld", r);
    }
    return r;
}

int64_t HlReadFile(Handle* hptr, void* __restrict buf, size_t count) {
    SDPRINTF("reading file: handle=%p, buffer=%p, count=%zu", hptr, buf, count);

    VALID_HNDL(hptr, HANDLE_TYPE_FILEIO, return -1)

    if (hptr->Payload.File.OpenFlags & FLAG_FILE_LOAD_MEMORY) {
        size_t off = hptr->Payload.File.FileOffset;
        size_t max = hptr->Payload.File.FileSize;
        if (off >= max) {
            SDPRINTF("read at EOF: offset=%zu, filesize=%zu", off, max);
            return 0;
        }

        size_t to_copy = count;
        if (off + to_copy > max)
            to_copy = max - off;

        mem::memcpy(
            buf,
            (uint8_t*)hptr->Payload.File.MappedAddress + off,
            to_copy
        );
        hptr->Payload.File.FileOffset += to_copy;
        SDPRINTF("read from mapped file: bytes=%zu, new_offset=%zu", to_copy, hptr->Payload.File.FileOffset);
        return to_copy;
    }

    int64_t r = vfs::read_file(
        hptr->Payload.File.FileDescriptor,
        buf,
        count
    );
    if (r > 0) {
        hptr->Payload.File.FileOffset += r;
        SDPRINTF("read successful: bytes=%ld, new_offset=%zu", r, hptr->Payload.File.FileOffset);
    } else {
        SDPRINTF("read failed: error=%ld", r);
    }
    return r;
}

int64_t HlPositionedWriteFile(
    Handle* hptr,
    size_t offset,
    const void* __restrict dat,
    size_t count
) {
    SDPRINTF("positioned write: handle=%p, offset=%zu, data=%p, count=%zu", hptr, offset, dat, count);

    VALID_HNDL(hptr, HANDLE_TYPE_FILEIO, return -1)

    if (hptr->Payload.File.OpenFlags & FLAG_FILE_LOAD_MEMORY) {
        size_t max = hptr->Payload.File.FileSize;
        if (offset >= max) {
            SDPRINTF("write beyond EOF: offset=%zu, filesize=%zu", offset, max);
            return 0;
        }

        size_t to_copy = count;
        if (offset + to_copy > max)
            to_copy = max - offset;

        mem::memcpy(
            (uint8_t*)hptr->Payload.File.MappedAddress + offset,
            dat,
            to_copy
        );
        SDPRINTF("positioned write to mapped file: bytes=%zu at offset=%zu", to_copy, offset);
        return to_copy;
    }

    return vfs::pwrite_file(
        hptr->Payload.File.FileDescriptor,
        offset,
        dat,
        count
    );
}

int64_t HlPositionedReadFile(
    Handle* hptr,
    size_t offset,
    void* __restrict buf,
    size_t count
) {
    SDPRINTF("positioned read: handle=%p, offset=%zu, buffer=%p, count=%zu", hptr, offset, buf, count);

    VALID_HNDL(hptr, HANDLE_TYPE_FILEIO, return -1)

    if (hptr->Payload.File.OpenFlags & FLAG_FILE_LOAD_MEMORY) {
        size_t max = hptr->Payload.File.FileSize;
        if (offset >= max) {
            SDPRINTF("read at/beyond EOF: offset=%zu, filesize=%zu", offset, max);
            return 0;
        }

        size_t to_copy = count;
        if (offset + to_copy > max)
            to_copy = max - offset;

        mem::memcpy(
            buf,
            (uint8_t*)hptr->Payload.File.MappedAddress + offset,
            to_copy
        );
        SDPRINTF("positioned read from mapped file: bytes=%zu at offset=%zu", to_copy, offset);
        return to_copy;
    }

    return vfs::pread_file(
        hptr->Payload.File.FileDescriptor,
        offset,
        buf,
        count
    );
}

void HlSyncFile(Handle* hptr) {
    SDPRINTF("syncing file: handle=%p", hptr);
    
    VALID_HNDL(hptr, HANDLE_TYPE_FILEIO, return)

    if (hptr->Payload.File.OpenFlags & FLAG_FILE_LOAD_MEMORY) {
        int64_t current_offset = vfs::seek_file(hptr->Payload.File.FileDescriptor, 0, SEEK_CUR);
        vfs::seek_file(hptr->Payload.File.FileDescriptor, 0, SEEK_SET);
        
        int64_t written = vfs::write_file(
            hptr->Payload.File.FileDescriptor,
            hptr->Payload.File.MappedAddress,
            hptr->Payload.File.FileSize
        );
        
        vfs::seek_file(hptr->Payload.File.FileDescriptor, current_offset, SEEK_SET);
        
        hptr->Payload.File.FileSize = vfs::stat_sz(hptr->Payload.File.FileDescriptor);
        
        SDPRINTF("file synced: bytes_written=%ld, new_size=%zu", written, hptr->Payload.File.FileSize);
    } else {
        vfs::sync_file(hptr->Payload.File.FileDescriptor);
    }
}

void HlOpenDirectory(Handle* hptr, const char* __restrict path, uint32_t OpenFlags) {
    SDPRINTF("opening directory: handle=%p, path='%s', flags=0x%x", hptr, path, OpenFlags);
    
    (void)OpenFlags;

    if (hptr->HandleType != HANDLE_TYPE_GENERIC) {
        SDPRINTF("invalid handle type: expected GENERIC, got %d", hptr->HandleType);
        return;
    }

    int dfd = vfs::open_dir(path, OpenFlags);
    if (dfd < 0) {
        SDPRINTF("open_dir failed: path='%s', error=%d", path, dfd);
        hptr->LastError = -1;
        return;
    }

    hptr->HandleType = HANDLE_TYPE_DIRIO;
    hptr->Flags = 0;
    hptr->RefCount = 1;
    hptr->OwnerPID = 0;
    hptr->LastError = 0;

    hptr->Payload.Directory.DirPath = path;
    hptr->Payload.Directory.DirDescriptor = dfd;
    hptr->Payload.Directory.ReadOffset = 0;
    hptr->Payload.Directory.EntryCountCache = 0;
    
    SDPRINTF("directory opened: dfd=%d", dfd);
}

void HlCloseDirectory(Handle* hptr) {
    SDPRINTF("closing directory: handle=%p", hptr);
    
    VALID_HNDL(hptr, HANDLE_TYPE_DIRIO, return)

    int dfd = hptr->Payload.Directory.DirDescriptor;
    vfs::close_dir(dfd);
    SDPRINTF("directory closed: dfd=%d", dfd);

    hptr->Payload.Directory.DirDescriptor = 0;
    hptr->HandleType = HANDLE_TYPE_GENERIC;
}

void HlMakeDirectory(const char* __restrict path) {
    SDPRINTF("creating directory: path='%s'", path);
    
    vfs::make_dir(path);
    
    SDPRINTF("directory creation initiated: path='%s'", path);
}

void HlRemoveDirectory(const char* __restrict path) {
    SDPRINTF("removing directory: path='%s'", path);
    
    vfs::remove_dir(path);
    
    SDPRINTF("directory removal initiated: path='%s'", path);
}

void HlListDirectory(Handle* hptr, void* __restrict buf) {
    SDPRINTF("listing directory: handle=%p, buffer=%p", hptr, buf);
    
    VALID_HNDL(hptr, HANDLE_TYPE_DIRIO, return)

    int dfd = hptr->Payload.Directory.DirDescriptor;
    if (dfd < 0) {
        SDPRINTF("invalid directory descriptor");
        return;
    }

    vfs::list_dir(dfd, buf);
    
    uint64_t* count = (uint64_t*)buf;
    hptr->Payload.Directory.ReadOffset += *count;
    
    SDPRINTF("directory listed: entries=%lu, total_offset=%lu", *count, hptr->Payload.Directory.ReadOffset);
}

void HlResetDirectoryReadOffset(Handle* hptr) {
    SDPRINTF("resetting directory read offset: handle=%p", hptr);
    
    VALID_HNDL(hptr, HANDLE_TYPE_DIRIO, return)

    int dfd = hptr->Payload.Directory.DirDescriptor;
    if (dfd < 0) {
        SDPRINTF("invalid directory descriptor");
        return;
    }

    vfs::reset_dir_read_off(dfd);
    hptr->Payload.Directory.ReadOffset = 0;
    
    SDPRINTF("directory read offset reset");
}

void* HlMemoryPoolAllocate(size_t n) {
    SDPRINTF("allocating from memory pool: bytes=%zu", n);
    
    size_t npages = (n + 0xFFF) / 0x1000;
    void* addr = mem::usr::alloc(npages);
    
    SDPRINTF("memory pool allocated: addr=%p, pages=%zu", addr, npages);
    return addr;
}

void HlMemoryPoolFree(void* ptr) {
    SDPRINTF("freeing memory pool: addr=%p", ptr);
    
    mem::usr::free(ptr, 1);
}

struct Pool {
    void* pool_base;
    size_t nbytes;
    size_t npages;
};

void* HlMemoryAllocatePool(size_t nbytes) {
    SDPRINTF("allocating memory pool: bytes=%zu", nbytes);
    
    size_t meta_pages = (sizeof(Pool) + 0xFFF) / 0x1000;
    Pool* newpool = (Pool*)mem::usr::alloc(meta_pages);

    newpool->npages = (nbytes + 0xFFF) / 0x1000;
    newpool->pool_base = mem::usr::alloc(newpool->npages);
    newpool->nbytes = nbytes;

    SDPRINTF("memory pool created: pool=%p, base=%p, pages=%zu", 
             newpool, newpool->pool_base, newpool->npages);
    return (void*)newpool;
}

void HlMemoryFreePool(void* poolptr) {
    SDPRINTF("freeing memory pool: pool=%p", poolptr);
    
    Pool* pool = (Pool*)poolptr;
    
    SDPRINTF("freeing pool data: base=%p, pages=%zu", pool->pool_base, pool->npages);
    mem::usr::free(pool->pool_base, pool->npages);
    mem::usr::free(pool, (sizeof(Pool) + 0xFFF) / 0x1000);
}

void* HlMemoryAllocateAligned(size_t npages) {
    SDPRINTF("allocating aligned memory: pages=%zu", npages);
    
    void* addr = mem::usr::alloc(npages);
    
    SDPRINTF("aligned memory allocated: addr=%p, pages=%zu", addr, npages);
    return addr;
}

void HlMemoryFreeAligned(void* ptr, size_t npages) {
    SDPRINTF("freeing aligned memory: addr=%p, pages=%zu", ptr, npages);
    
    mem::usr::free(ptr, npages);
}

void HlMemorySetAttributes(void* ptr, size_t npages, uint64_t attributes) {
    SDPRINTF("setting memory attributes: addr=%p, pages=%zu, attrs=0x%lx", ptr, npages, attributes);
    
    mem::vmm::mmap(ptr, ptr, npages, attributes | PAGE_USER | PAGE_PRESENT);
}

// returns the process ID
int64_t HlCreateNewProcess() {
    SDPRINTF("creating new process (stub)");
    
    return -1; // NO-OP
}

void HlKillProcess(int64_t pid) {
    SDPRINTF("killing process (stub): pid=%ld", pid);
    
    (void)pid;
}

void HlTerminateProcess(int64_t pid) {
    SDPRINTF("terminating process (stub): pid=%ld", pid);
    
    (void)pid;
}

int HlExec(const char* __restrict path, const char* args[], const char* env_vars[]) {
    SDPRINTF("executing: path='%s'", path);
    
    return -1;
}

void HlExit(int error_code) {
    SDPRINTF("exit called: code=%d", error_code);
    
    (void)error_code;
    Log::warnf("HlExit is a stub");
    while (1) asm ("hlt");
}

void HlOpenConsole(Handle* portR, Handle* portW) {
    SDPRINTF("opening console: portR=%p, portW=%p", portR, portW);
    
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
    
    SDPRINTF("console opened successfully");
}

void HlWaitForInputConsole(Handle* portR) {
    SDPRINTF("waiting for console input: handle=%p", portR);
    
    VALID_HNDL(portR, HANDLE_TYPE_CONSOLE_R, return)
    while (!drivers::tty::ldisc::has_input());
    
    SDPRINTF("console input available");
}

int64_t HlReadConsole(Handle* portW, void* __restrict buf, size_t count) {
    SDPRINTF("reading console: handle=%p, buffer=%p, count=%zu", portW, buf, count);
    
    VALID_HNDL(portW, HANDLE_TYPE_CONSOLE_R, return -1)
    int64_t bytes = drivers::tty::ldisc::read(true, (char*)buf, count);
    
    SDPRINTF("console read: bytes=%ld", bytes);
    return bytes;
}

int64_t HlWriteConsole(Handle* portR, const void* __restrict dat, size_t count) {
    SDPRINTF("writing console: handle=%p, data=%p, count=%zu", portR, dat, count);
    
    VALID_HNDL(portR, HANDLE_TYPE_CONSOLE_W, return -1)
    int64_t bytes = drivers::tty::ldisc::write((const char*)dat, count);
    
    SDPRINTF("console written: bytes=%ld", bytes);
    return bytes;
}

extern "C" uint64_t g_scr_height, g_scr_width;

#define QUICK_FB_ACCESS hptr->Payload.Framebuffer
void HlObtainFramebuffer(Handle* hptr) {
    SDPRINTF("obtaining framebuffer: handle=%p", hptr);
    
    VALID_HNDL(hptr, HANDLE_TYPE_GENERIC, return);

    hptr->HandleType = HANDLE_TYPE_FRAMEBUFFER;

    hptr->RefCount = 1;
    hptr->OwnerPID = 0;
    hptr->LastError = 0;

    QUICK_FB_ACCESS.BaseAddress = (void*)get_base_fb();
    QUICK_FB_ACCESS.Width = g_scr_width;;
    QUICK_FB_ACCESS.Height = g_scr_height;
    QUICK_FB_ACCESS.Pitch = get_pitch();
    QUICK_FB_ACCESS.BitsPerPixel = get_bpp();
    QUICK_FB_ACCESS.Stride = get_stride();
    
    SDPRINTF("framebuffer obtained: addr=%p, %lux%lu, bpp=%lu", 
             QUICK_FB_ACCESS.BaseAddress, QUICK_FB_ACCESS.Width, 
             QUICK_FB_ACCESS.Height, QUICK_FB_ACCESS.BitsPerPixel);
}
#undef QUICK_FB_ACCESS

void HlStatFramebuffer(Handle* hptr, void* buf) {
    SDPRINTF("stat framebuffer: handle=%p, buffer=%p", hptr, buf);
    
    VALID_HNDL(hptr, HANDLE_TYPE_FRAMEBUFFER, buf = nullptr; return);

    mem::memcpy(buf, &hptr->Payload.Framebuffer, sizeof(void*) + (5*sizeof(uint64_t)));
    
    SDPRINTF("framebuffer stats copied");
}

void* HlRetrieveFileMappedMemory(Handle* hptr) {
    SDPRINTF("retrieving file mapped memory: handle=%p", hptr);
    
    VALID_HNDL(hptr, HANDLE_TYPE_FILEIO, return nullptr);
    
    void* addr = hptr->Payload.File.MappedAddress;
    SDPRINTF("mapped memory retrieved: addr=%p", addr);
    return addr;
}

uint64_t HlRetrieveMappedFileSize(Handle* hptr) {
    SDPRINTF("retrieving mapped file size: handle=%p", hptr);
    
    VALID_HNDL(hptr, HANDLE_TYPE_FILEIO, return 0);

    uint64_t size = hptr->Payload.File.MappedSize;
    SDPRINTF("mapped file size: %lu pages", size);
    return size;
}

void HlPrintInt64(uint64_t int_) {
    SDPRINTF("printing integer: value=%lu", int_);
    
	printf("%lu\n\r", int_);
}
