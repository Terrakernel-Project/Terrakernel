#ifndef SYSCALLS_H
#define SYSCALLS_H 1

#include <stddef.h>
#include <stdint.h>

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR 0x0002
#define O_CREAT 0x0040
#define O_EXCL 0x0080
#define O_TRUNC 0x0200
#define O_APPEND 0x0400
#define O_DIRECTORY 0x10000

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define DT_UNKNOWN 0
#define DT_REG 8
#define DT_DIR 4

#define NAME_MAX 255
#define PATH_MAX 4096

struct stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint64_t st_rdev;
    uint64_t st_size;
    uint64_t st_blksize;
    uint64_t st_blocks;
    uint64_t st_atime;
    uint64_t st_mtime;
    uint64_t st_ctime;
};

struct dirent {
    uint64_t d_ino;
    uint64_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[NAME_MAX + 1];
};

typedef struct {
    void* internal;
    uint64_t pos;
} DIR;

struct timespec {
    long tv_sec;
    long tv_nsec;
};

typedef unsigned int mode_t;
typedef uint64_t fd_t;
typedef uint64_t off_t;
typedef signed long long ssize_t;
typedef uint32_t pid_t;

#define SYS_read        0
#define SYS_write       1
#define SYS_open        2
#define SYS_close       3
#define SYS_stat        4
#define SYS_fstat       5
#define SYS_lstat       6
#define SYS_lseek       7
#define SYS_brk         8
#define SYS_dup         9
#define SYS_dup2        10
#define SYS_nanosleep   11
#define SYS_getpid      12
#define SYS_fork        13
#define SYS_vfork       14
#define SYS_execve      15
#define SYS_exit        16
#define SYS_truncate    17
#define SYS_ftruncate   18
#define SYS_rename      19
#define SYS_mkdir       20
#define SYS_rmdir       21
#define SYS_reboot      22

static inline uint64_t syscall0(uint64_t n) {
    uint64_t ret;
    asm volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall1(uint64_t n, uint64_t a1) {
    uint64_t ret;
    asm volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall2(uint64_t n, uint64_t a1, uint64_t a2) {
    uint64_t ret;
    asm volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall3(uint64_t n, uint64_t a1, uint64_t a2, uint64_t a3) {
    uint64_t ret;
    asm volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall4(uint64_t n, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
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

static inline uint64_t syscall5(uint64_t n, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    uint64_t ret;
    register uint64_t r10 asm("r10") = a4;
    register uint64_t r8  asm("r8")  = a5;
    asm volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall6(uint64_t n, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    uint64_t ret;
    register uint64_t r10 asm("r10") = a4;
    register uint64_t r8  asm("r8")  = a5;
    register uint64_t r9  asm("r9")  = a6;
    asm volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline ssize_t sys_read(fd_t fd, char* buf, size_t count) {
    return syscall3(SYS_read, fd, (uint64_t)buf, count);
}

static inline ssize_t sys_write(fd_t fd, const char* buf, size_t count) {
    return syscall3(SYS_write, fd, (uint64_t)buf, count);
}

static inline fd_t sys_open(const char* filename, int flags, mode_t mode) {
    return syscall3(SYS_open, (uint64_t)filename, flags, mode);
}

static inline int sys_close(fd_t fd) {
    return syscall1(SYS_close, fd);
}

static inline int sys_stat(const char* filename, struct stat* buf) {
    return syscall2(SYS_stat, (uint64_t)filename, (uint64_t)buf);
}

static inline int sys_fstat(fd_t fd, struct stat* buf) {
    return syscall2(SYS_fstat, fd, (uint64_t)buf);
}

static inline int sys_lstat(const char* filename, struct stat* buf) {
    return syscall2(SYS_lstat, (uint64_t)filename, (uint64_t)buf);
}

static inline off_t sys_lseek(fd_t fd, off_t offset, unsigned int whence) {
    return syscall3(SYS_lseek, fd, offset, whence);
}

static inline int sys_brk(void* addr) {
    return syscall1(SYS_brk, (uint64_t)addr);
}

static inline int sys_dup(fd_t fd) {
    return syscall1(SYS_dup, fd);
}

static inline int sys_dup2(fd_t oldfd, fd_t newfd) {
    return syscall2(SYS_dup2, oldfd, newfd);
}

static inline int sys_nanosleep(struct timespec* rqtp, struct timespec* rmtp) {
    return syscall2(SYS_nanosleep, (uint64_t)rqtp, (uint64_t)rmtp);
}

static inline pid_t sys_getpid() {
    return syscall0(SYS_getpid);
}

static inline pid_t sys_fork() {
    return syscall0(SYS_fork);
}

static inline pid_t sys_vfork() {
    return syscall0(SYS_vfork);
}

static inline int sys_execve(const char* filename, const char** argv, const char** envp) {
    return syscall3(SYS_execve, (uint64_t)filename, (uint64_t)argv, (uint64_t)envp);
}

static inline void sys_exit(int code) {
    syscall1(SYS_exit, code);
}

static inline int sys_truncate(const char* path, long length) {
    return syscall2(SYS_truncate, (uint64_t)path, length);
}

static inline int sys_ftruncate(fd_t fd, off_t length) {
    return syscall2(SYS_ftruncate, fd, length);
}

static inline int sys_rename(const char* oldname, const char* newname) {
    return syscall2(SYS_rename, (uint64_t)oldname, (uint64_t)newname);
}

static inline int sys_mkdir(const char* pathname, mode_t mode) {
    return syscall2(SYS_mkdir, (uint64_t)pathname, mode);
}

static inline int sys_rmdir(const char* pathname) {
    return syscall1(SYS_rmdir, (uint64_t)pathname);
}

static inline int sys_reboot(int magic1, int magic2, uint32_t cmd, void* arg) {
    return syscall4(SYS_reboot, magic1, magic2, cmd, (uint64_t)arg);
}

#endif
