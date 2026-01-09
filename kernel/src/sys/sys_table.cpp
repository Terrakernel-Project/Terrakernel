#define IN_SYS_SRC

#include "sys.hpp"

#include <ramfs/ramfs.hpp>
#include <types.hpp>
#include <error.hpp>
#include <proc/proc.hpp>
#include <drivers/timers/apic/apic.hpp>
#include <uacpi/uacpi.h>
#include <uacpi/sleep.h>
#include <cstdio>

ssize_t sys_read(fd_t fd, char* buf, size_t count) {
    return ramfs::read(fd, buf, count);
}

ssize_t sys_write(fd_t fd, const char* buf, size_t count) {
    if (fd == 1 || fd == 2) {
        for (size_t curr = 0; curr < count; curr++) {
            putchar(buf[curr]);
        }
    }
    return ramfs::write(fd, buf, count);
}

fd_t sys_open(const char* filename, int flags, mode_t mode) {
    return ramfs::open(filename, flags, mode);
}

int sys_close(fd_t fd) {
    return ramfs::close(fd);
}

int sys_stat(const char* filename, stat* statbuf) {
    return ramfs::stat(filename, statbuf);
}

int sys_fstat(fd_t fd, stat* statbuf) {
    return ramfs::fstat(fd, statbuf);
}

int sys_lstat(const char* filename, stat* statbuf) {
    (void)filename; (void)statbuf;
    return -ENOSYS;
}

off_t sys_lseek(fd_t fd, off_t offset, unsigned int whence) {
    return ramfs::lseek(fd, offset, whence);
}

int sys_brk(void* addr) {
    return proc::brk(addr);
}

int sys_dup(fd_t fildes) {
    return ramfs::dup(fildes);
}

int sys_dup2(fd_t oldfd, fd_t newfd) {
    return ramfs::dup2(oldfd, newfd);
}

int sys_nanosleep(timespec* rqtp, timespec* rmtp) {
    (void)rmtp;
    if (rqtp->tv_nsec == 0 && rqtp->tv_sec == 0) return 0;
    else if (rqtp->tv_nsec == 0 && rqtp->tv_sec != 0) {
    	drivers::timers::apic::sleep_ms(rqtp->tv_sec * 1000);
    } else if (rqtp->tv_nsec != 0 && rqtp->tv_sec == 0) {
    	uint64_t ms = rqtp->tv_sec * 1000 + rqtp->tv_nsec / 1000000;
    	drivers::timers::apic::sleep_ms(ms);
    } else {
    	drivers::timers::apic::sleep_ms(rqtp->tv_sec * 1000);
    }
    return 0;
}

pid_t sys_getpid() {
    return 0;
}

pid_t sys_fork() {
    return proc::fork();
}

pid_t sys_vfork() {
    return proc::vfork();
}

int sys_execve(const char* filename, const char** argv, const char** envp) {
    int argc = 0;
    while (argv && argv[argc]) argc++;
    proc::execve(filename, argc, (char**)argv, (char**)envp);
    return 0;
}

void sys_exit(int error_code) {
    proc::exit(error_code);
}

int sys_truncate(const char* path, long length) {
    return ramfs::truncate(path, length);
}

int sys_ftruncate(fd_t fd, off_t length) {
    return ramfs::ftruncate(fd, length);
}

int sys_rename(const char* oldname, const char* newname) {
    return ramfs::rename(oldname, newname);
}

int sys_mkdir(const char* pathname, mode_t mode) {
    return ramfs::mkdir(pathname, mode);
}

int sys_rmdir(const char* pathname) {
    return ramfs::rmdir(pathname);
}

#define LINUX_REBOOT_CMD_RESTART    0x01234567
#define LINUX_REBOOT_CMD_HALT       0xCDEF0123
#define LINUX_REBOOT_CMD_CAD_ON     0x89ABCDEF
#define LINUX_REBOOT_CMD_CAD_OFF    0x00000000
#define LINUX_REBOOT_CMD_POWER_OFF  0x4321FEDC
#define LINUX_REBOOT_CMD_RESTART2   0xA1B2C3D4
#define LINUX_REBOOT_CMD_SW_SUSPEND 0xD000FCE2
#define LINUX_REBOOT_CMD_KEXEC      0x45584543

int acpi_restart() {
    uacpi_status ret = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S4);
    if (uacpi_unlikely_error(ret)) return -EIO;
    asm("cli");
    ret = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S4);
    if (uacpi_unlikely_error(ret)) return -EIO;
    return 0;
}

int acpi_poweroff() {
    uacpi_status ret = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_MAX);
    if (uacpi_unlikely_error(ret)) return -EIO;
    asm("cli");
    ret = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_MAX);
    if (uacpi_unlikely_error(ret)) return -EIO;
    return 0;
}

int sys_reboot(int magic1, int magic2, uint32_t cmd, void* arg) {
    (void)magic1; (void)magic2; (void)arg;
    switch (cmd) {
        case LINUX_REBOOT_CMD_RESTART: return acpi_restart();
        case LINUX_REBOOT_CMD_POWER_OFF:
        default: return acpi_poweroff();
    }
}
