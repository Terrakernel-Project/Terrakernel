#define IN_SYS_SRC

#include "sys.hpp"
#include <arch/x86_64/syscall/handlers.hpp>
#include <error.hpp>
#include "sys_num.hpp"

int64_t no_handler() {
	return -ENOSYS;
}

void register_syscall(uint64_t vector,
					void* handler,
					uint64_t num_args,
					const char* name,
					const char* cpp_pretty_func);

void set_no_handler(uint64_t vector) {
	register_syscall(vector, (void*)no_handler, 0, "no_handler", "int64_t no_handler()");
}

void initialise_syscalls() {
    register_syscall(SYS_read,      (void*)sys_read,      3, "sys_read",      "ssize_t sys_read(fd_t fd, char* buf, size_t count)");
    register_syscall(SYS_write,     (void*)sys_write,     3, "sys_write",     "ssize_t sys_write(fd_t fd, const char* buf, size_t count)");
    register_syscall(SYS_open,      (void*)sys_open,      3, "sys_open",      "fd_t sys_open(const char* filename, int flags, mode_t mode)");
    register_syscall(SYS_close,     (void*)sys_close,     1, "sys_close",     "int sys_close(fd_t fd)");
    register_syscall(SYS_stat,      (void*)sys_stat,      2, "sys_stat",      "int sys_stat(const char* filename, stat* statbuf)");
    register_syscall(SYS_fstat,     (void*)sys_fstat,     2, "sys_fstat",     "int sys_fstat(fd_t fd, stat* statbuf)");
    register_syscall(SYS_lstat,     (void*)sys_lstat,     2, "sys_lstat",     "int sys_lstat(const char* filename, stat* statbuf)");
    register_syscall(SYS_brk,       (void*)sys_brk,       1, "sys_brk",       "int sys_brk(uint32_t brk)");
    register_syscall(SYS_dup,       (void*)sys_dup,       1, "sys_dup",       "int sys_dup(fd_t fildes)");
    register_syscall(SYS_dup2,      (void*)sys_dup2,      2, "sys_dup2",      "int sys_dup2(fd_t oldfd, fd_t newfd)");
    register_syscall(SYS_nanosleep, (void*)sys_nanosleep, 2, "sys_nanosleep", "int sys_nanosleep(timespec* rqtp, timespec* rmtp)");
    register_syscall(SYS_getpid,    (void*)sys_getpid,    0, "sys_getpid",    "pid_t sys_getpid()");
    register_syscall(SYS_fork,      (void*)sys_fork,      0, "sys_fork",      "pid_t sys_fork()");
    register_syscall(SYS_vfork,     (void*)sys_vfork,     0, "sys_vfork",     "pid_t sys_vfork()");
    register_syscall(SYS_execve,    (void*)sys_execve,    3, "sys_execve",    "int sys_execve(const char* filename, const char* argv, const char* envp)");
    register_syscall(SYS_exit,      (void*)sys_exit,      1, "sys_exit",      "void sys_exit(int error_code)");
    register_syscall(SYS_truncate,  (void*)sys_truncate,  2, "sys_truncate",  "int sys_truncate(const char* path, long length)");
    register_syscall(SYS_ftruncate, (void*)sys_ftruncate, 2, "sys_ftruncate", "int sys_ftruncate(fd_t fd, off_t length)");
    register_syscall(SYS_rename,    (void*)sys_rename,    2, "sys_rename",    "int sys_rename(const char* oldname, const char* newname)");
    register_syscall(SYS_mkdir,     (void*)sys_mkdir,     2, "sys_mkdir",     "int sys_mkdir(const char* pathname, mode_t mode)");
    register_syscall(SYS_rmdir,     (void*)sys_rmdir,     1, "sys_rmdir",     "int sys_rmdir(const char* pathname)");
    register_syscall(SYS_reboot,    (void*)sys_reboot,    4, "sys_reboot",    "int sys_reboot(int magic1, int magic2, uint32_t cmd, void* arg)");
}

