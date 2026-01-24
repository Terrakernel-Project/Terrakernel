#ifndef OBJECT_MANAGER_HPP
#define OBJECT_MANAGER_HPP 1

#include <cstdint>

#define HANDLE_TYPE_GENERIC     0
#define HANDLE_TYPE_FILEIO      1
#define HANDLE_TYPE_DIRIO       2
#define HANDLE_TYPE_CONSOLE_R   3
#define HANDLE_TYPE_CONSOLE_W   4
#define HANDLE_TYPE_PROC        5
#define HANDLE_TYPE_FRAMEBUFFER 6

#define FLAG_FILE_LOAD_MEMORY    (1 << 31)
#define FLAG_FILE_NEEDS_SYNC     (1 << 30)

#define FLAG_MEMORY_READ           (1 << 16)
#define FLAG_MEMORY_WRITE          (1 << 17)
#define FLAG_MEMORY_EXECUTE        (1 << 18)

#define PROC_STATE_EMBRYO          0
#define PROC_STATE_RUNNING         1
#define PROC_STATE_ZOMBIE          2
#define PROC_STATE_TERMINATED      3

typedef struct objman_handle {
    uint32_t HandleType;
    uint32_t Flags;
    uint64_t RefCount;
    uint32_t OwnerPID;

    uint64_t ObjectID;

    int32_t  LastError;

    union {
        struct {
            const char* FilePath;
            int64_t     FileDescriptor;
            uint64_t    FileOffset;
            uint64_t    FileSize;

            void       *MappedAddress;
            uint64_t    MappedSize;

            uint32_t    OpenFlags;
            uint32_t    FileSystemID;
        } File;

        struct {
            const char* DirPath;
            int64_t     DirDescriptor;
            uint64_t    ReadOffset;
            uint32_t    EntryCountCache;
            uint32_t    Reserved;
        } Directory;

        struct {
            uint32_t    ConsoleID;
            uint32_t    Mode;
            uint64_t    LineBufferSize;
            void       *LineBuffer;
        } Console;

        struct {
            uint32_t    ProcessID;
            uint32_t    ParentPID;
            uint32_t    ProtectionLevel;

            uint32_t    State;
            uint64_t    ExitCode;

            void       *AddressSpace;
            void       *MainThread;
        } Process;

        struct {
            void       *BaseAddress;
            uint64_t    PoolSize;
            uint64_t    UsedSize;
        } MemoryPool;

        struct {
            void       *BaseAddress;
            uint64_t    Width;
            uint64_t    Height;
            uint64_t    Pitch;
            uint64_t    BitsPerPixel;
            uint64_t    Stride;
        } Framebuffer;

    } Payload;
} Handle;

namespace ObjMan {

Handle* CreateNewHandle();
void DestroyHandle(Handle* HandlePtr);
bool ValidateID(uint64_t objid);

}

#endif