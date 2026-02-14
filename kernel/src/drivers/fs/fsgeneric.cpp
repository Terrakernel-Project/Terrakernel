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
    current_fs_type = fs_type::FS_TYPE_FAT32;
    
    switch (current_fs_type) {
        case fs_type::FS_TYPE_FAT32:
            fat32_init();
            break;
        case fs_type::FS_TYPE_HLFS:
            break;
    }
}

fgfile_t* open_disk_file_image(const char* __restrict full_path) {
    if (current_fs_type != fs_type::FS_TYPE_FAT32) {
        return nullptr;
    }
    
    fgfile_t* file = (fgfile_t*)mem::heap::malloc(sizeof(fgfile_t));
    if (!file) {
        return nullptr;
    }
    
    mem::memset(file, 0, sizeof(fgfile_t));
    file->part_sn = 0;
    
    uint32_t cluster;
    bool is_directory;
    
    if (!fat32_navigate_path(full_path, &cluster, &is_directory)) {
        mem::heap::free(file);
        return nullptr;
    }
    
    if (is_directory) {
        mem::heap::free(file);
        return nullptr;
    }
    
    const char* filename = full_path;
    for (int i = strlen(full_path) - 1; i >= 0; i--) {
        if (full_path[i] == '/') {
            filename = &full_path[i + 1];
            break;
        }
    }
    
    size_t filename_len = strlen(filename);
    if (filename_len > 127) {
        filename_len = 127;
    }
    mem::memcpy(file->file_name_ascii, filename, filename_len);
    file->file_name_ascii[filename_len] = '\0';
    
    char* dir_path = (char*)mem::heap::malloc(strlen(full_path) + 1);
    strcpy(dir_path, full_path);
    
    char* last_slash = nullptr;
    for (int i = strlen(dir_path) - 1; i >= 0; i--) {
        if (dir_path[i] == '/') {
            last_slash = &dir_path[i];
            break;
        }
    }
    
    uint32_t dir_cluster;
    if (last_slash && last_slash != dir_path) {
        *last_slash = '\0';
        if (!fat32_navigate_path(dir_path, &dir_cluster, &is_directory)) {
            mem::heap::free(dir_path);
            mem::heap::free(file);
            return nullptr;
        }
    } else {
        dir_cluster = 2;
    }
    
    mem::heap::free(dir_path);
    
    fat32_dir_entry entry;
    uint32_t file_cluster;
    
    if (!fat32_find_file_in_directory(dir_cluster, filename, &entry, &file_cluster)) {
        mem::heap::free(file);
        return nullptr;
    }
    
    file->file_size = entry.file_size;
    file->first_lba = fat32_cluster_to_lba(file_cluster);
    file->last_load_address = nullptr;
    file->last_load_size = 0;
    
    return file;
}

void close_disk_file_image(fgfile_t* file) {
    if (!file) {
        return;
    }
    
    if (file->last_load_address) {
        mem::heap::free(file->last_load_address);
        file->last_load_address = nullptr;
    }
    
    mem::heap::free(file);
}

int64_t read_file_contents(fgfile_t* file, void* __restrict buffer, size_t buffer_size) {
    if (!file || !buffer) {
        return -1;
    }
    
    if (current_fs_type != fs_type::FS_TYPE_FAT32) {
        return -1;
    }
    
    char* file_path = (char*)mem::heap::malloc(256);
    strcpy(file_path, "/");
    strcat(file_path, file->file_name_ascii);
    
    uint32_t cluster;
    bool is_directory;
    
    if (!fat32_navigate_path(file_path, &cluster, &is_directory)) {
        mem::heap::free(file_path);
        return -1;
    }
    
    mem::heap::free(file_path);
    
    if (!fat32_read_file(cluster, file->file_size, (uint8_t*)buffer, buffer_size)) {
        return -1;
    }
    
    file->last_load_address = buffer;
    file->last_load_size = file->file_size;
    
    return file->file_size;
}

int64_t write_file_contents(fgfile_t* file, const void* __restrict data, size_t data_size) {
    if (!file || !data) {
        return -1;
    }
    
    if (current_fs_type != fs_type::FS_TYPE_FAT32) {
        return -1;
    }
    
    char* file_path = (char*)mem::heap::malloc(256);
    strcpy(file_path, "/");
    strcat(file_path, file->file_name_ascii);
    
    uint32_t cluster;
    bool is_directory;
    
    if (!fat32_navigate_path(file_path, &cluster, &is_directory)) {
        mem::heap::free(file_path);
        return -1;
    }
    
    mem::heap::free(file_path);
    
    if (!fat32_write_file(cluster, (const uint8_t*)data, data_size)) {
        return -1;
    }
    
    return data_size;
}

bool list_directory(const char* path, fgfile_t* entries[], size_t* count, size_t max_entries) {
    if (current_fs_type != fs_type::FS_TYPE_FAT32) {
        return false;
    }
    
    uint32_t dir_cluster;
    bool is_directory;
    
    if (!fat32_navigate_path(path, &dir_cluster, &is_directory)) {
        return false;
    }
    
    if (!is_directory) {
        return false;
    }
    
    fat32_dir_entry* dir_entries = (fat32_dir_entry*)mem::heap::malloc(
        sizeof(fat32_dir_entry) * max_entries
    );
    
    if (!fat32_read_directory_entries(dir_cluster, dir_entries, count, max_entries)) {
        mem::heap::free(dir_entries);
        return false;
    }
    
    for (size_t i = 0; i < *count; i++) {
        fgfile_t* file = (fgfile_t*)mem::heap::malloc(sizeof(fgfile_t));
        mem::memset(file, 0, sizeof(fgfile_t));
        
        fat32_83_to_filename(dir_entries[i].name, file->file_name_ascii);
        
        file->file_size = dir_entries[i].file_size;
        uint32_t cluster = ((uint32_t)dir_entries[i].first_cluster_hi << 16) | 
                          dir_entries[i].first_cluster_lo;
        file->first_lba = fat32_cluster_to_lba(cluster);
        file->part_sn = 0;
        file->last_load_address = nullptr;
        file->last_load_size = 0;
        
        entries[i] = file;
    }
    
    mem::heap::free(dir_entries);
    return true;
}

bool create_directory(const char* path) {
    if (current_fs_type != fs_type::FS_TYPE_FAT32) {
        return false;
    }
    
    char* dir_path = (char*)mem::heap::malloc(strlen(path) + 1);
    strcpy(dir_path, path);
    
    char* last_slash = nullptr;
    for (int i = strlen(dir_path) - 1; i >= 0; i--) {
        if (dir_path[i] == '/') {
            last_slash = &dir_path[i];
            break;
        }
    }
    
    if (!last_slash) {
        mem::heap::free(dir_path);
        return false;
    }
    
    const char* dirname = last_slash + 1;
    *last_slash = '\0';
    
    uint32_t parent_cluster;
    bool is_directory;
    
    if (strlen(dir_path) == 0) {
        parent_cluster = 2;
    } else {
        if (!fat32_navigate_path(dir_path, &parent_cluster, &is_directory)) {
            mem::heap::free(dir_path);
            return false;
        }
        
        if (!is_directory) {
            mem::heap::free(dir_path);
            return false;
        }
    }
    
    uint32_t new_cluster;
    bool result = fat32_create_directory(parent_cluster, dirname, &new_cluster);
    
    mem::heap::free(dir_path);
    return result;
}

bool delete_file(const char* path) {
    if (current_fs_type != fs_type::FS_TYPE_FAT32) {
        return false;
    }
    
    uint32_t cluster;
    bool is_directory;
    
    if (!fat32_navigate_path(path, &cluster, &is_directory)) {
        return false;
    }
    
    if (is_directory) {
        return false;
    }
    
    char* dir_path = (char*)mem::heap::malloc(strlen(path) + 1);
    strcpy(dir_path, path);
    
    char* last_slash = nullptr;
    for (int i = strlen(dir_path) - 1; i >= 0; i--) {
        if (dir_path[i] == '/') {
            last_slash = &dir_path[i];
            break;
        }
    }
    
    if (!last_slash) {
        mem::heap::free(dir_path);
        return false;
    }
    
    const char* filename = last_slash + 1;
    *last_slash = '\0';
    
    uint32_t dir_cluster;
    
    if (strlen(dir_path) == 0) {
        dir_cluster = 2;
    } else {
        if (!fat32_navigate_path(dir_path, &dir_cluster, &is_directory)) {
            mem::heap::free(dir_path);
            return false;
        }
    }
    
    mem::heap::free(dir_path);
    
    if (!fat32_mark_entry_deleted(dir_cluster, filename)) {
        return false;
    }
    
    if (!fat32_free_cluster_chain(cluster)) {
        return false;
    }
    
    return true;
}

bool rename_file(const char* old_path, const char* new_name) {
    if (current_fs_type != fs_type::FS_TYPE_FAT32) {
        return false;
    }
    
    uint32_t cluster;
    bool is_directory;
    
    if (!fat32_navigate_path(old_path, &cluster, &is_directory)) {
        return false;
    }
    
    char* dir_path = (char*)mem::heap::malloc(strlen(old_path) + 1);
    strcpy(dir_path, old_path);
    
    char* last_slash = nullptr;
    for (int i = strlen(dir_path) - 1; i >= 0; i--) {
        if (dir_path[i] == '/') {
            last_slash = &dir_path[i];
            break;
        }
    }
    
    if (!last_slash) {
        mem::heap::free(dir_path);
        return false;
    }
    
    const char* old_filename = last_slash + 1;
    *last_slash = '\0';
    
    uint32_t dir_cluster;
    
    if (strlen(dir_path) == 0) {
        dir_cluster = 2;
    } else {
        if (!fat32_navigate_path(dir_path, &dir_cluster, &is_directory)) {
            mem::heap::free(dir_path);
            return false;
        }
    }
    
    mem::heap::free(dir_path);
    
    fat32_dir_entry entry;
    uint32_t file_cluster;
    
    if (!fat32_find_file_in_directory(dir_cluster, old_filename, &entry, &file_cluster)) {
        return false;
    }
    
    fat32_filename_to_83(new_name, entry.name);
    
    bool result = fat32_update_dir_entry(dir_cluster, old_filename, &entry);
    return result;
}

bool move_file(const char* src_path, const char* dst_path) {
    if (current_fs_type != fs_type::FS_TYPE_FAT32) {
        return false;
    }
    
    uint32_t src_cluster;
    bool is_directory;
    
    if (!fat32_navigate_path(src_path, &src_cluster, &is_directory)) {
        return false;
    }
    
    char* src_dir_path = (char*)mem::heap::malloc(strlen(src_path) + 1);
    strcpy(src_dir_path, src_path);
    
    char* src_last_slash = nullptr;
    for (int i = strlen(src_dir_path) - 1; i >= 0; i--) {
        if (src_dir_path[i] == '/') {
            src_last_slash = &src_dir_path[i];
            break;
        }
    }
    
    if (!src_last_slash) {
        mem::heap::free(src_dir_path);
        return false;
    }
    
    const char* src_filename = src_last_slash + 1;
    *src_last_slash = '\0';
    
    uint32_t src_dir_cluster;
    if (strlen(src_dir_path) == 0) {
        src_dir_cluster = 2;
    } else {
        if (!fat32_navigate_path(src_dir_path, &src_dir_cluster, &is_directory)) {
            mem::heap::free(src_dir_path);
            return false;
        }
    }
    
    mem::heap::free(src_dir_path);
    
    fat32_dir_entry src_entry;
    uint32_t file_cluster;
    
    if (!fat32_find_file_in_directory(src_dir_cluster, src_filename, &src_entry, &file_cluster)) {
        return false;
    }
    
    char* dst_dir_path = (char*)mem::heap::malloc(strlen(dst_path) + 1);
    strcpy(dst_dir_path, dst_path);
    
    char* dst_last_slash = nullptr;
    for (int i = strlen(dst_dir_path) - 1; i >= 0; i--) {
        if (dst_dir_path[i] == '/') {
            dst_last_slash = &dst_dir_path[i];
            break;
        }
    }
    
    if (!dst_last_slash) {
        mem::heap::free(dst_dir_path);
        return false;
    }
    
    const char* dst_filename = dst_last_slash + 1;
    *dst_last_slash = '\0';
    
    uint32_t dst_dir_cluster;
    if (strlen(dst_dir_path) == 0) {
        dst_dir_cluster = 2;
    } else {
        if (!fat32_navigate_path(dst_dir_path, &dst_dir_cluster, &is_directory)) {
            mem::heap::free(dst_dir_path);
            return false;
        }
    }
    
    mem::heap::free(dst_dir_path);
    
    if (!fat32_mark_entry_deleted(src_dir_cluster, src_filename)) {
        return false;
    }
    
    uint32_t entry_cluster, entry_offset;
    if (!fat32_find_free_entry(dst_dir_cluster, &entry_cluster, &entry_offset)) {
        return false;
    }
    
    fat32_filename_to_83(dst_filename, src_entry.name);
    
    if (!fat32_write_dir_entry(entry_cluster, entry_offset, &src_entry)) {
        return false;
    }
    
    return true;
}

bool copy_file(const char* src_path, const char* dst_path) {
    if (current_fs_type != fs_type::FS_TYPE_FAT32) {
        return false;
    }
    
    fgfile_t* src_file = open_disk_file_image(src_path);
    if (!src_file) {
        return false;
    }
    
    uint8_t* buffer = (uint8_t*)mem::heap::malloc(src_file->file_size);
    if (!buffer) {
        close_disk_file_image(src_file);
        return false;
    }
    
    int64_t bytes_read = read_file_contents(src_file, buffer, src_file->file_size);
    if (bytes_read != (int64_t)src_file->file_size) {
        mem::heap::free(buffer);
        close_disk_file_image(src_file);
        return false;
    }
    
    close_disk_file_image(src_file);
    
    char* dst_dir_path = (char*)mem::heap::malloc(strlen(dst_path) + 1);
    strcpy(dst_dir_path, dst_path);
    
    char* last_slash = nullptr;
    for (int i = strlen(dst_dir_path) - 1; i >= 0; i--) {
        if (dst_dir_path[i] == '/') {
            last_slash = &dst_dir_path[i];
            break;
        }
    }
    
    if (!last_slash) {
        mem::heap::free(dst_dir_path);
        mem::heap::free(buffer);
        return false;
    }
    
    const char* dst_filename = last_slash + 1;
    *last_slash = '\0';
    
    uint32_t dst_dir_cluster;
    bool is_directory;
    
    if (strlen(dst_dir_path) == 0) {
        dst_dir_cluster = 2;
    } else {
        if (!fat32_navigate_path(dst_dir_path, &dst_dir_cluster, &is_directory)) {
            mem::heap::free(dst_dir_path);
            mem::heap::free(buffer);
            return false;
        }
    }
    
    mem::heap::free(dst_dir_path);
    
    uint32_t new_cluster;
    if (!fat32_create_file(dst_dir_cluster, dst_filename, &new_cluster)) {
        mem::heap::free(buffer);
        return false;
    }
    
    if (!fat32_write_file(new_cluster, buffer, bytes_read)) {
        mem::heap::free(buffer);
        return false;
    }
    
    mem::heap::free(buffer);
    return true;
}

bool create_file(const char* path) {
    if (current_fs_type != fs_type::FS_TYPE_FAT32) {
        return false;
    }
    
    char* dir_path = (char*)mem::heap::malloc(strlen(path) + 1);
    strcpy(dir_path, path);
    
    char* last_slash = nullptr;
    for (int i = strlen(dir_path) - 1; i >= 0; i--) {
        if (dir_path[i] == '/') {
            last_slash = &dir_path[i];
            break;
        }
    }
    
    if (!last_slash) {
        mem::heap::free(dir_path);
        return false;
    }
    
    const char* filename = last_slash + 1;
    *last_slash = '\0';
    
    uint32_t dir_cluster;
    bool is_directory;
    
    if (strlen(dir_path) == 0) {
        dir_cluster = 2;
    } else {
        if (!fat32_navigate_path(dir_path, &dir_cluster, &is_directory)) {
            mem::heap::free(dir_path);
            return false;
        }
    }
    
    mem::heap::free(dir_path);
    
    uint32_t new_cluster;
    return fat32_create_file(dir_cluster, filename, &new_cluster);
}

bool file_exists(const char* path) {
    uint32_t cluster;
    bool is_directory;
    return fat32_navigate_path(path, &cluster, &is_directory);
}

bool is_directory_path(const char* path) {
    uint32_t cluster;
    bool is_directory;
    if (!fat32_navigate_path(path, &cluster, &is_directory)) {
        return false;
    }
    return is_directory;
}

}
