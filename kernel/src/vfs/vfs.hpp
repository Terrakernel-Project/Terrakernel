#ifndef VFS_HPP
#define VFS_HPP 1

#include <cstdint>
#include <cstddef>

namespace vfs {

void initialise();

int open(const char* __restrict path, uint32_t open_flags);
void close(int fd);

uint64_t stat_sz(int fd);
int64_t seek_file(int fd, int64_t offset, int whence);

int64_t write_file(int fd, const void* __restrict data, size_t count);
int64_t read_file(int fd, void* __restrict buf, size_t count);
int64_t pwrite_file(int fd, size_t offset, const void* __restrict dat, size_t count);
int64_t pread_file(int fd, size_t offset, void* __restrict buf, size_t count);

void sync_file(int fd);

int open_dir(const char* __restrict path, uint32_t open_flags);
void close_dir(int fd);

void make_dir(const char* __restrict path);
void remove_dir(const char* __restrict path);
void list_dir(int dfd, void* __restrict buf);
void reset_dir_read_off(int dfd);

}

#endif
