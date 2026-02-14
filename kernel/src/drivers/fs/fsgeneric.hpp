#ifndef FSGENERIC_HPP
#define FSGENERIC_HPP 1

#include <cstdint>
#include <cstddef>

typedef struct fsgeneric_file {
    char file_name_ascii[128];
    size_t file_size;
    uint64_t first_lba;
    int part_sn;
    void* last_load_address;
    size_t last_load_size;
} fgfile_t;

namespace drivers::fs::fsgeneric {

void initialise(bool is_full_disk_fs);

fgfile_t* open_disk_file_image(const char* __restrict full_path);
void close_disk_file_image(fgfile_t* file);
int64_t read_file_contents(fgfile_t* file, void* __restrict buffer, size_t buffer_size);
int64_t write_file_contents(fgfile_t* file, const void* __restrict data, size_t data_size);

bool list_directory(const char* path, fgfile_t* entries[], size_t* count, size_t max_entries);
bool create_directory(const char* path);
bool delete_file(const char* path);

bool file_exists(const char* path);
bool is_directory_path(const char* path);

bool rename_file(const char* old_path, const char* new_name);
bool move_file(const char* src_path, const char* dst_path);
bool copy_file(const char* src_path, const char* dst_path);
bool create_file(const char* path);

}

#endif
