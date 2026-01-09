#ifndef RAMFS_HPP
#define RAMFS_HPP 1

#include <cstdint>
#include <cstddef>

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR 0x0002
#define O_CREAT 0x0040
#define O_EXCL 0x0080
#define O_TRUNC 0x0200
#define O_APPEND 0x0400
#define O_DIRECTORY 0x10000

#define S_IFMT 0170000
#define S_IFDIR 0040000
#define S_IFREG 0100000
#define S_IRUSR 0000400
#define S_IWUSR 0000200
#define S_IXUSR 0000100
#define S_IRGRP 0000040
#define S_IWGRP 0000020
#define S_IXGRP 0000010
#define S_IROTH 0000004
#define S_IWOTH 0000002
#define S_IXOTH 0000001

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

namespace ramfs {

void initialise();

int open(const char* pathname, int flags, uint32_t mode = 0777);
int close(int fd);
int64_t read(int fd, void* buf, size_t count);
int64_t write(int fd, const void* buf, size_t count);
int64_t lseek(int fd, int64_t offset, int whence);
int stat(const char* pathname, struct stat* statbuf);
int fstat(int fd, struct stat* statbuf);
int mkdir(const char* pathname, uint32_t mode);
int rmdir(const char* pathname);
int unlink(const char* pathname);
int link(const char* oldpath, const char* newpath);
int rename(const char* oldpath, const char* newpath);
DIR* opendir(const char* name);
struct dirent* readdir(DIR* dirp);
int closedir(DIR* dirp);
int chdir(const char* path);
char* getcwd(char* buf, size_t size);
int truncate(const char* path, uint64_t length);
int ftruncate(int fd, uint64_t length);
int access(const char* pathname, int mode);
int chmod(const char* pathname, uint32_t mode);
int dup(int oldfd);
int dup2(int oldfd, int newfd);

int load_archive(const char* type, void* base, size_t size, const char* path_prefix);

}

#define LOAD_ARCHIVE_TYPE_USTAR "USTAR"
#define LOAD_ARCHIVE_TYPE_TAR "TAR"
/* TODO: add more types */

#endif
