# Terra Syscall API Specification v1.0

## Syscall Convention
- **Syscall number**: RAX
- **Arguments**: RDI, RSI, RDX, R10, R8, R9 (SysV ABI style)
- **Return value**: RAX (negative on error, see error codes)
- **Preserved registers**: RBX, RBP, R12-R15

## Error Codes
```c
#define ESUCCESS     0   // Success
#define EINVAL      -1   // Invalid argument
#define ENOENT      -2   // No such file or directory
#define EACCES      -3   // Permission denied
#define ENOMEM      -4   // Out of memory
#define EBADF       -5   // Bad file descriptor
#define EEXIST      -6   // File exists
#define ENOTDIR     -7   // Not a directory
#define EISDIR      -8   // Is a directory
#define EAGAIN      -9   // Try again
#define ENOSYS     -10   // Function not implemented
```

---

## Process Management

### sys_exit
```c
// Syscall #0
void sys_exit(int status);
```
Terminates the calling process with the given exit status.
- **status**: Exit code to return to parent

---

### sys_spawn
```c
// Syscall #1
int sys_spawn(const char *path, const char **argv, const char **envp);
```
Creates a new process from an executable file.
- **path**: Path to executable
- **argv**: Null-terminated argument array
- **envp**: Null-terminated environment array
- **Returns**: Process ID on success, negative error code on failure

---

### sys_wait
```c
// Syscall #2
int sys_wait(int pid, int *status);
```
Waits for a child process to terminate.
- **pid**: Process ID to wait for (-1 for any child)
- **status**: Pointer to store exit status (can be NULL)
- **Returns**: PID of terminated child, or negative error code

---

### sys_getpid
```c
// Syscall #3
int sys_getpid(void);
```
Returns the process ID of the calling process.
- **Returns**: Current process ID

---

## Thread Management

### sys_thread_create
```c
// Syscall #4
int sys_thread_create(void *entry, void *arg, void *stack);
```
Creates a new thread in the current process.
- **entry**: Thread entry point function
- **arg**: Argument to pass to entry function
- **stack**: Stack pointer for new thread (NULL for kernel-allocated)
- **Returns**: Thread ID on success, negative error code on failure

---

### sys_thread_exit
```c
// Syscall #5
void sys_thread_exit(void *retval);
```
Terminates the calling thread.
- **retval**: Return value for thread

---

## Memory Management

### sys_mmap
```c
// Syscall #6
void *sys_mmap(void *addr, size_t length, int prot, int flags);
```
Maps memory into the process address space.
- **addr**: Desired address (NULL for kernel choice)
- **length**: Size of mapping in bytes
- **prot**: Protection flags (PROT_READ | PROT_WRITE | PROT_EXEC)
- **flags**: Mapping flags (MAP_PRIVATE | MAP_SHARED | MAP_ANON)
- **Returns**: Mapped address on success, negative error code on failure

**Protection flags:**
```c
#define PROT_NONE   0x0
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4
```

**Mapping flags:**
```c
#define MAP_PRIVATE  0x01  // Private copy-on-write mapping
#define MAP_SHARED   0x02  // Shared mapping
#define MAP_ANON     0x04  // Anonymous mapping (no file)
#define MAP_FIXED    0x08  // Require exact address
```

---

### sys_munmap
```c
// Syscall #7
int sys_munmap(void *addr, size_t length);
```
Unmaps a previously mapped memory region.
- **addr**: Starting address of region
- **length**: Size of region in bytes
- **Returns**: 0 on success, negative error code on failure

---

### sys_mprotect
```c
// Syscall #8
int sys_mprotect(void *addr, size_t length, int prot);
```
Changes protection flags on a memory region.
- **addr**: Starting address
- **length**: Size of region
- **prot**: New protection flags
- **Returns**: 0 on success, negative error code on failure

---

## File I/O & VFS

### sys_open
```c
// Syscall #9
int sys_open(const char *path, int flags, int mode);
```
Opens a file and returns a file descriptor.
- **path**: Path to file
- **flags**: Open flags (O_RDONLY | O_WRONLY | O_RDWR | O_CREAT | O_TRUNC | O_APPEND)
- **mode**: Permissions for newly created files (0644, etc.)
- **Returns**: File descriptor on success, negative error code on failure

**Open flags:**
```c
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0100
#define O_TRUNC     0x0200
#define O_APPEND    0x0400
#define O_DIRECTORY 0x0800
#define O_BUILTIN_DEVICE_FILE 0x1000  // Terra-specific: for /dev/* files
```

---

### sys_close
```c
// Syscall #10
int sys_close(int fd);
```
Closes a file descriptor.
- **fd**: File descriptor to close
- **Returns**: 0 on success, negative error code on failure

---

### sys_read
```c
// Syscall #11
ssize_t sys_read(int fd, void *buf, size_t count);
```
Reads data from a file descriptor.
- **fd**: File descriptor
- **buf**: Buffer to read into
- **count**: Maximum bytes to read
- **Returns**: Bytes read on success, 0 on EOF, negative error code on failure

---

### sys_write
```c
// Syscall #12
ssize_t sys_write(int fd, const void *buf, size_t count);
```
Writes data to a file descriptor.
- **fd**: File descriptor
- **buf**: Buffer containing data to write
- **count**: Number of bytes to write
- **Returns**: Bytes written on success, negative error code on failure

---

### sys_seek
```c
// Syscall #13
off_t sys_seek(int fd, off_t offset, int whence);
```
Changes the file position.
- **fd**: File descriptor
- **offset**: Offset to seek to
- **whence**: Reference point (SEEK_SET | SEEK_CUR | SEEK_END)
- **Returns**: New file position on success, negative error code on failure

**Whence values:**
```c
#define SEEK_SET  0  // From beginning of file
#define SEEK_CUR  1  // From current position
#define SEEK_END  2  // From end of file
```

---

### sys_stat
```c
// Syscall #14
int sys_stat(const char *path, struct stat *buf);
```
Gets file information.
- **path**: Path to file
- **buf**: Buffer to fill with file info
- **Returns**: 0 on success, negative error code on failure

```c
struct stat {
    uint64_t st_dev;      // Device ID
    uint64_t st_ino;      // Inode number
    uint32_t st_mode;     // File type and mode
    uint32_t st_nlink;    // Number of hard links
    uint32_t st_uid;      // User ID
    uint32_t st_gid;      // Group ID
    uint64_t st_size;     // File size in bytes
    uint64_t st_blksize;  // Block size
    uint64_t st_blocks;   // Number of blocks
    uint64_t st_atime;    // Last access time
    uint64_t st_mtime;    // Last modification time
    uint64_t st_ctime;    // Last status change time
};
```

---

### sys_fstat
```c
// Syscall #15
int sys_fstat(int fd, struct stat *buf);
```
Gets file information from file descriptor.
- **fd**: File descriptor
- **buf**: Buffer to fill with file info
- **Returns**: 0 on success, negative error code on failure

---

### sys_readdir
```c
// Syscall #16
int sys_readdir(int fd, struct dirent *buf, size_t count);
```
Reads directory entries.
- **fd**: File descriptor for directory
- **buf**: Buffer for directory entries
- **count**: Maximum number of entries to read
- **Returns**: Number of entries read, 0 on end, negative on error

```c
struct dirent {
    uint64_t d_ino;       // Inode number
    uint64_t d_off;       // Offset to next dirent
    uint16_t d_reclen;    // Length of this record
    uint8_t  d_type;      // File type
    char     d_name[256]; // Filename (null-terminated)
};
```

---

### sys_ioctl
```c
// Syscall #17
int sys_ioctl(int fd, unsigned long request, void *arg);
```
Device-specific I/O control operations.
- **fd**: File descriptor (typically for /dev/* devices)
- **request**: Device-specific request code
- **arg**: Pointer to request-specific data
- **Returns**: 0 on success, negative error code on failure

---

## IPC (Inter-Process Communication)

### sys_ipc_send
```c
// Syscall #18
int sys_ipc_send(int target_pid, const void *msg, size_t len, int flags);
```
Sends a message to another process.
- **target_pid**: Destination process ID
- **msg**: Message buffer
- **len**: Message length
- **flags**: Send flags (IPC_NOWAIT for non-blocking)
- **Returns**: 0 on success, negative error code on failure

---

### sys_ipc_receive
```c
// Syscall #19
ssize_t sys_ipc_receive(int *sender_pid, void *buf, size_t maxlen, int flags);
```
Receives a message from another process.
- **sender_pid**: Pointer to store sender's PID (can be NULL)
- **buf**: Buffer to receive message
- **maxlen**: Maximum message length
- **flags**: Receive flags (IPC_NOWAIT for non-blocking)
- **Returns**: Message length on success, negative error code on failure

---

### sys_ipc_call
```c
// Syscall #20
ssize_t sys_ipc_call(int target_pid, const void *request, size_t req_len, 
                     void *response, size_t resp_maxlen);
```
Synchronous IPC call (send + receive in one syscall).
- **target_pid**: Target process ID
- **request**: Request message buffer
- **req_len**: Request length
- **response**: Response buffer
- **resp_maxlen**: Maximum response length
- **Returns**: Response length on success, negative error code on failure

**IPC flags:**
```c
#define IPC_NOWAIT  0x01  // Don't block on send/receive
```

---

## System Information

### sys_debug
```c
// Syscall #21
int sys_debug(const char *msg);
```
Outputs a debug message to kernel console.
- **msg**: Null-terminated debug message
- **Returns**: 0 on success, negative error code on failure

*Note: This syscall is temporary for debugging and will be removed in future versions.*

---

### sys_sleep
```c
// Syscall #22
int sys_sleep(uint64_t nanoseconds);
```
Suspends execution for a specified duration.
- **nanoseconds**: Time to sleep in nanoseconds
- **Returns**: 0 on success, negative error code on failure

---

### sys_gettime
```c
// Syscall #23
uint64_t sys_gettime(int clock_id);
```
Gets the current time.
- **clock_id**: Clock type (CLOCK_REALTIME | CLOCK_MONOTONIC)
- **Returns**: Time in nanoseconds since epoch/boot

**Clock types:**
```c
#define CLOCK_REALTIME   0  // Wall clock time
#define CLOCK_MONOTONIC  1  // Monotonic time since boot
```

---

## Total Syscalls: 24 (0-23)

## Notes
- All pointers passed from userspace are validated by the kernel
- Syscall numbers are stable and will not change
- Unimplemented syscalls return -ENOSYS
- For hybrid kernel design, driver communication uses IPC syscalls rather than expanding the syscall table
