#include "fsgeneric.hpp"
#include "fat32/fat32.hpp"
#include <mem/mem.hpp>
#include <string.h>

enum class fs_type {
    FS_TYPE_FAT32,
    FS_TYPE_HLFS,
};

static fs_type current_fs_type;

namespace drivers::fs::fsgeneric {

void initialise(bool is_full_disk_fs) {
	// TODO: allow different filesystems
    current_fs_type = fs_type::FS_TYPE_FAT32;
    
    switch (current_fs_type) {
        case fs_type::FS_TYPE_FAT32:
            fat_init();
            break;
        case fs_type::FS_TYPE_HLFS:
            break;
    }
}

fgfile_t* open_disk_file_image(const char* __restrict full_path) {
    return nullptr;
}

void close_disk_file_image(fgfile_t* file) {
}

int64_t read_file_contents(fgfile_t* file, void* __restrict buffer, size_t buffer_size) {
	return -1;
}

int64_t write_file_contents(fgfile_t* file, const void* __restrict data, size_t data_size) {
    return -1;
}

bool list_directory(const char* path, fgfile_t* entries[], size_t* count, size_t max_entries) {
    return false;
}

bool create_directory(const char* path) {
    return false;
}

bool delete_file(const char* path) {
    return false;
}

bool rename_file(const char* old_path, const char* new_name) {
    return false;
}

bool move_file(const char* src_path, const char* dst_path) {
    return false;
}

bool copy_file(const char* src_path, const char* dst_path) {
    return false;
}

bool create_file(const char* path) {
    return false;
}

bool file_exists(const char* path) {
    return false;
}

bool is_directory_path(const char* path) {
    return false;
}

}
