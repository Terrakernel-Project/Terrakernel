#include "vfs.hpp"
#include <drivers/fs/fat32/fat32.hpp>
#include <cstring>
#include <mem/mem.hpp>
#include <subsystems/ramfs/ramfs.hpp>
#include <cstdio>
#include <config.hpp>

#ifdef CONFIG_VFS_VERBOSE
#	define VDPRINTF(fmt, ...) printf("[ %s ] " fmt, __PRETTY_FUNCTION__, ##__VA_ARGS__)
#else
#	define VDPRINTF(fmt, ...)
#endif

struct vfs_mountpoint {
	char* mountpoint_name;
	char* alt_name;
	bool is_physical_fs;
};

struct vfs_fd {
	int fd;
	int mountpoint_id;
	union {
		int ramfs_fd;
		char* phys_path;
	} payload;
	uint64_t seek_off;
	uint64_t dir_seek;
	bool allocated;
	bool dir;
} fdtable[1024];

int allocated_fds = 0;

int get_fd_num() {
	for (int fd = 0; fd < 1024; fd++) {
		bool used = false;
		for (int i = 0; i < 1024; i++) {
			if (fdtable[i].allocated && fdtable[i].fd == fd) {
				used = true;
				break;
			}
		}
		if (!used)
			return fd;
	}
	return -1;
}

vfs_fd* allocate_fd() {
	for (int i = 0; i < 1024; i++) {
		if (!fdtable[i].allocated) {
			mem::memset(&fdtable[i], 0, sizeof(vfs_fd));
			fdtable[i].allocated = true;
			fdtable[i].fd = get_fd_num();
			VDPRINTF("Allocated slot %d -> fd %d\r\n", i, fdtable[i].fd);
			return &fdtable[i];
		}
	}
	VDPRINTF("Failed to get a file descriptor\r\n");
	return nullptr;
}

void free_fd(vfs_fd* fd) {
	if (!fd) return;
	VDPRINTF("Freeing fd %d\r\n", fd->fd);
	fd->allocated = false;
}

vfs_fd* get_struc_from_fd(int fd) {
	if (fd <= -1) return nullptr;
	for (int i = 0; i < 1024; i++) {
		if (fdtable[i].fd == fd && fdtable[i].allocated) return &fdtable[i];
	}
	VDPRINTF("Failed to get file desciptor from fd-num (%d)\r\n", fd);
	VDPRINTF("Dumping allocated fds:\r\n");
	for (int i = 0; i < 1024; i++) {
		if (fdtable[i].allocated) {
			VDPRINTF("  slot %d: fd=%d mountpoint=%d dir=%d\r\n",
				i, fdtable[i].fd, fdtable[i].mountpoint_id, fdtable[i].dir);
		}
	}
	return nullptr;
}

#define MAX_MOUNTPOINTS        2
#define MOUNTPOINT_PHYS_DISK   0
#define MOUNTPOINT_RAMFS_DISK  1

vfs_mountpoint mountpoints[MAX_MOUNTPOINTS];

// paths: MOUNT_POINT:PATH

bool parse_path(const char* path, int& mountpoint_id) {
	const char* colon = strchr(path, ':');
	if (!colon) return false;
	size_t len = colon - path;
	for (int i = 0; i < MAX_MOUNTPOINTS; i++) {
		if (!strncmp(path, mountpoints[i].mountpoint_name, len) ||
			!strncmp(path, mountpoints[i].alt_name, len)) {
			mountpoint_id = i;
			return true;
		}
	}
	return false;
}

char* skip_mountpoint(const char* path) {
	char* colon = strchr((char*)path, ':');
	return colon ? colon + 1 : nullptr;
}

namespace vfs {

void initialise() {
	mountpoints[MOUNTPOINT_RAMFS_DISK].mountpoint_name = "ramfs";
	mountpoints[MOUNTPOINT_RAMFS_DISK].alt_name = "ram";
	mountpoints[MOUNTPOINT_RAMFS_DISK].is_physical_fs = false;

	mountpoints[MOUNTPOINT_PHYS_DISK].mountpoint_name = "a";
	mountpoints[MOUNTPOINT_PHYS_DISK].alt_name = "A";
	mountpoints[MOUNTPOINT_PHYS_DISK].is_physical_fs = true;
}

int open(const char* __restrict path, uint32_t open_flags) {
	VDPRINTF("path='%s' flags=0x%x\r\n", path, open_flags);
	char* _p = skip_mountpoint(path);
	if (!_p) _p = (char*)path;
	int mid;
	if (!parse_path(path, mid)) mid = MOUNTPOINT_PHYS_DISK;
	VDPRINTF("resolved path='%s' mountpoint=%d\r\n", _p, mid);
	vfs_fd* _f = allocate_fd();
	if (!_f) return -1;
	switch (mid) {
		case MOUNTPOINT_PHYS_DISK: {
			fat_file_info inf;
			if (!fat_stat_file(_p, &inf)) {
				VDPRINTF("fat_stat_file failed for '%s'\r\n", _p);
				free_fd(_f);
				return -1;
			}
			VDPRINTF("fat_stat_file ok size=%llu\r\n", inf.size);
			_f->payload.phys_path = strdup(_p);
			if (!_f->payload.phys_path) {
				free_fd(_f);
				return -1; // i think if strdup fails mem::heap::malloc panics automatically
			}
			_f->seek_off = 0;
			_f->mountpoint_id = mid;
			VDPRINTF("opened phys fd=%d\r\n", _f->fd);
			return _f->fd;
		}
		case MOUNTPOINT_RAMFS_DISK: {
			_f->payload.ramfs_fd = ramfs::open(_p, open_flags); // HlApi expected to remove the flags not used here
			if (_f->payload.ramfs_fd <= -1) {
				VDPRINTF("ramfs::open failed for '%s'\r\n", _p);
				free_fd(_f);
				return -1;
			}
			VDPRINTF("ramfs::open inner fd=%d\r\n", _f->payload.ramfs_fd);
			stat statbuf;
			ramfs::fstat(_f->payload.ramfs_fd, &statbuf);
			if ((statbuf.st_mode & S_IFMT) == S_IFDIR) {
				VDPRINTF("path is a directory, rejecting\r\n");
				free_fd(_f);
				return -1;
			}
			_f->seek_off = 0;
			_f->mountpoint_id = mid;
			VDPRINTF("opened ramfs fd=%d size=%lld\r\n", _f->fd, statbuf.st_size);
			return _f->fd;
		}
	}
	return -1;
}

void close(int fd) {
	VDPRINTF("fd=%d\r\n", fd);
	vfs_fd* _f = get_struc_from_fd(fd);
	if (!_f) return;
	switch (_f->mountpoint_id) {
		case MOUNTPOINT_RAMFS_DISK:
			ramfs::close(_f->payload.ramfs_fd);
			break;
	}
	free_fd(_f);
}

uint64_t stat_sz(int fd) {
	VDPRINTF("fd=%d\r\n", fd);
	vfs_fd* _f = get_struc_from_fd(fd);
	if (!_f) return (uint64_t)-1;
	switch (_f->mountpoint_id) {
		case MOUNTPOINT_PHYS_DISK: {
			fat_file_info inf;
			if (!fat_stat_file(_f->payload.phys_path, &inf)) {
				VDPRINTF("fat_stat_file failed\r\n");
				return (uint64_t)-1;
			}
			VDPRINTF("phys size=%llu\r\n", inf.size);
			return inf.size;
		}
		case MOUNTPOINT_RAMFS_DISK: {
			stat statbuf;
			if (ramfs::fstat(_f->payload.ramfs_fd, &statbuf) == -1) {
				VDPRINTF("ramfs fstat failed\r\n");
				return (uint64_t)-1;
			}
			VDPRINTF("ramfs size=%lld\r\n", statbuf.st_size);
			return statbuf.st_size;
		}
	}
	return (uint64_t)-1;
}

int64_t seek_file(int fd, int64_t offset, int whence) {
	VDPRINTF("fd=%d offset=%lld whence=%d\r\n", fd, offset, whence);
	vfs_fd* _f = get_struc_from_fd(fd);
	if (!_f) return -1;
	switch (_f->mountpoint_id) {
		case MOUNTPOINT_PHYS_DISK: {
			uint64_t sz = stat_sz(fd);
			int64_t new_off = 0;
			switch (whence) {
				case SEEK_SET: new_off = offset; break;
				case SEEK_CUR: new_off = (int64_t)_f->seek_off + offset; break;
				case SEEK_END: new_off = (int64_t)sz + offset; break;
				default: return -1;
			}
			if (new_off < 0) new_off = 0;
			if ((uint64_t)new_off > sz) new_off = sz;
			_f->seek_off = new_off;
			VDPRINTF("new offset=%lld\r\n", new_off);
			return new_off;
		}
		case MOUNTPOINT_RAMFS_DISK:
			return ramfs::lseek(_f->payload.ramfs_fd, offset, whence);
		default:
			return -1;
	}
	return -1;
}

int64_t write_file(int fd, const void* __restrict data, size_t count) {
	VDPRINTF("fd=%d count=%zu\r\n", fd, count);
	vfs_fd* _f = get_struc_from_fd(fd);
	if (!_f) return -1;
	switch (_f->mountpoint_id) {
		case MOUNTPOINT_PHYS_DISK:
			return fat_write_file(_f->payload.phys_path, data, count, _f->seek_off);
		case MOUNTPOINT_RAMFS_DISK:
			return ramfs::write(_f->payload.ramfs_fd, data, count);
	}
	return -1;
}

int64_t read_file(int fd, void* __restrict buf, size_t count) {
	VDPRINTF("fd=%d count=%zu\r\n", fd, count);
	vfs_fd* _f = get_struc_from_fd(fd);
	if (!_f) return -1;
	VDPRINTF("mountpoint=%d seek_off=%llu\r\n", _f->mountpoint_id, _f->seek_off);
	switch (_f->mountpoint_id) {
		case MOUNTPOINT_PHYS_DISK: {
			int64_t ret = fat_read_file(_f->payload.phys_path, buf, count, _f->seek_off);
			VDPRINTF("fat_read_file returned %lld\r\n", ret);
			if (ret > 0) _f->seek_off += ret;
			return ret;
		}
		case MOUNTPOINT_RAMFS_DISK: {
			int64_t ret = ramfs::read(_f->payload.ramfs_fd, buf, count);
			VDPRINTF("ramfs read returned %lld\r\n", ret);
			return ret;
		}
	}
	return -1;
}

int64_t pwrite_file(int fd, size_t offset, const void* __restrict dat, size_t count) {
	VDPRINTF("fd=%d offset=%zu count=%zu\r\n", fd, offset, count);
	vfs_fd* _f = get_struc_from_fd(fd);
	if (!_f) return -1;
	switch (_f->mountpoint_id) {
		case MOUNTPOINT_PHYS_DISK:
			return fat_write_file(_f->payload.phys_path, dat, count, offset);
		case MOUNTPOINT_RAMFS_DISK: {
			int rfd = _f->payload.ramfs_fd;
			int64_t off = ramfs::tell(rfd);
			ramfs::lseek(rfd, offset, SEEK_SET);
			int64_t ret = ramfs::write(rfd, dat, count);
			ramfs::lseek(rfd, off, SEEK_SET);
			return ret;
		}
	}
	return -1;
}

int64_t pread_file(int fd, size_t offset, void* __restrict buf, size_t count) {
	VDPRINTF("fd=%d offset=%zu count=%zu\r\n", fd, offset, count);
	vfs_fd* _f = get_struc_from_fd(fd);
	if (!_f) return -1;
	switch (_f->mountpoint_id) {
		case MOUNTPOINT_PHYS_DISK:
			return fat_read_file(_f->payload.phys_path, buf, count, offset);
		case MOUNTPOINT_RAMFS_DISK: {
			int rfd = _f->payload.ramfs_fd;
			int64_t off = ramfs::tell(rfd);
			ramfs::lseek(rfd, offset, SEEK_SET);
			int64_t ret = ramfs::read(rfd, buf, count);
			ramfs::lseek(rfd, off, SEEK_SET);
			return ret;
		}
	}
	return -1;
}

void sync_file(int fd) {
	vfs_fd* _f = get_struc_from_fd(fd);
	if (!_f) return;
	// no need to sync, we dont have sync stuff
}

int open_dir(const char* __restrict path, uint32_t open_flags) {
	VDPRINTF("path='%s' flags=0x%x\r\n", path, open_flags);
	char* _p = skip_mountpoint(path);
	if (!_p) _p = (char*)path;
	int mid;
	if (!parse_path(path, mid)) mid = MOUNTPOINT_PHYS_DISK;
	VDPRINTF("resolved path='%s' mountpoint=%d\r\n", _p, mid);
	vfs_fd* _f = allocate_fd();
	if (!_f) return -1;
	switch (mid) {
		case MOUNTPOINT_PHYS_DISK: {
			fat_file_info inf;
			if (!fat_stat_dir(_p, &inf)) {
				VDPRINTF("fat_stat_dir failed for '%s'\r\n", _p);
				free_fd(_f);
				return -1;
			}
			_f->payload.phys_path = strdup(_p);
			if (!_f->payload.phys_path) {
				free_fd(_f);
				return -1; // i think if strdup fails mem::heap::malloc panics automatically
			}
			_f->dir_seek = 0;
			_f->mountpoint_id = mid;
			_f->dir = true;
			VDPRINTF("opened phys dir fd=%d\r\n", _f->fd);
			return _f->fd;
		}
		case MOUNTPOINT_RAMFS_DISK: {
			_f->payload.ramfs_fd = ramfs::open(_p, open_flags | O_DIRECTORY); // HlApi expected to remove the flags not used here
			if (_f->payload.ramfs_fd <= -1) {
				VDPRINTF("ramfs::open failed for '%s'\r\n", _p);
				free_fd(_f);
				return -1;
			}
			stat statbuf;
			ramfs::fstat(_f->payload.ramfs_fd, &statbuf);
			if ((statbuf.st_mode & S_IFMT) != S_IFDIR) {
				VDPRINTF("path is not a directory (mode=0x%x)\r\n", statbuf.st_mode);
				free_fd(_f);
				return -1;
			}
			_f->dir_seek = 0;
			_f->mountpoint_id = mid;
			_f->dir = true;
			VDPRINTF("opened ramfs dir fd=%d\r\n", _f->fd);
			return _f->fd;
		}
	}
	return -1;
}

void close_dir(int fd) {
	VDPRINTF("fd=%d\r\n", fd);
	vfs_fd* _f = get_struc_from_fd(fd);
	if (!_f) return;
	if (!_f->dir) return;
	switch (_f->mountpoint_id) {
		case MOUNTPOINT_RAMFS_DISK: {
			stat statbuf;
			ramfs::fstat(_f->payload.ramfs_fd, &statbuf);
			if ((statbuf.st_mode & S_IFMT) != S_IFDIR) {
				free_fd(_f);
				return;
			} // close_dir never meant to close files
			ramfs::close(_f->payload.ramfs_fd);
			break;
		}
	}
	free_fd(_f);
}

void make_dir(const char* __restrict path) {
	VDPRINTF("path='%s'\r\n", path);
	char* _p = skip_mountpoint(path);
	if (!_p) _p = (char*)path;
	int mid;
	if (!parse_path(path, mid)) mid = MOUNTPOINT_PHYS_DISK;
	switch (mid) {
		case MOUNTPOINT_PHYS_DISK:
			fat_make_dir(_p);
		case MOUNTPOINT_RAMFS_DISK:
			ramfs::mkdir(_p, 0777);
	}
}

void remove_dir(const char* __restrict path) {
	VDPRINTF("path='%s'\r\n", path);
	char* _p = skip_mountpoint(path);
	if (!_p) _p = (char*)path;
	int mid;
	if (!parse_path(path, mid)) mid = MOUNTPOINT_PHYS_DISK;
	switch (mid) {
		case MOUNTPOINT_PHYS_DISK:
			fat_remove_dir(_p);
		case MOUNTPOINT_RAMFS_DISK:
			ramfs::rmdir(_p);
	}
}

void list_dir(int dfd, void* __restrict buf) {
	VDPRINTF("dfd=%d\r\n", dfd);
	vfs_fd* _f = get_struc_from_fd(dfd);
	if (!_f) return;
	uint64_t* count = (uint64_t*)buf;
	char* base = (char*)buf + sizeof(uint64_t);
	*count = 0;
	switch (_f->mountpoint_id) {
		case MOUNTPOINT_PHYS_DISK:
			fat_read_dir(_f->payload.phys_path, &_f->dir_seek, 64, buf);
			break;
		case MOUNTPOINT_RAMFS_DISK: {
			DIR* dir = (DIR*)_f->payload.ramfs_fd;
			if (!dir) return;
			size_t skipped = 0;
			while (skipped < _f->dir_seek) {
				struct dirent* e = ramfs::readdir(dir);
				if (!e) break;
				skipped++;
			}
			for (size_t i = 0; i < 64; ++i) {
				struct dirent* ent = ramfs::readdir(dir);
				if (!ent) break;
				char* dst = base + i * (NAME_MAX + 1);
				strncpy(dst, ent->d_name, NAME_MAX);
				dst[NAME_MAX] = '\0';
				VDPRINTF("entry[%zu]='%s'\r\n", i, ent->d_name);
				(*count)++;
			}
			_f->dir_seek += 64;
			VDPRINTF("total entries=%llu\r\n", *count);
			break;
		}
	}
}

void reset_dir_read_off(int dfd) {
	VDPRINTF("dfd=%d\r\n", dfd);
	vfs_fd* _f = get_struc_from_fd(dfd);
	if (!_f) return;
	_f->dir_seek = 0;
}

}
