#ifndef SYS_HPP
#define SYS_HPP

#include <cstdint>
#include <types.hpp>

// only sys_*.cpp files could include the following file(s)
#ifdef IN_SYS_SRC
#include <arch/x86_64/syscall/handlers.hpp>
#include <ramfs/ramfs.hpp>
#endif

struct timespec {
    long tv_sec;
    long tv_nsec;
};

ssize_t sys_read(fd_t fd, char* buf, size_t count);
ssize_t sys_write(fd_t fd, const char* buf, size_t count);
fd_t sys_open(const char* filename, int flags, mode_t mode);
int sys_close(fd_t fd);
int sys_stat(const char* filename, stat* statbuf);
int sys_fstat(fd_t fd, stat* statbuf);
int sys_lstat(const char* filename, stat* statbuf);
off_t sys_lseek(fd_t fd, off_t offset, unsigned int whence);
int sys_brk(void* addr);
int sys_dup(fd_t fildes);
int sys_dup2(fd_t oldfd, fd_t newfd);
int sys_nanosleep(timespec* rqtp, timespec* rmtp);
pid_t sys_getpid();
pid_t sys_fork();
pid_t sys_vfork();
int sys_execve(const char* filename, const char** argv, const char** envp);
void sys_exit(int error_code);
int sys_truncate(const char* path, long length);
int sys_ftruncate(fd_t fd, off_t length);
int sys_rename(const char* oldname, const char* newname);
int sys_mkdir(const char* pathname, mode_t mode);
int sys_rmdir(const char* pathname);
int sys_reboot(int magic1, int magic2, uint32_t cmd, void* arg);

#endif
