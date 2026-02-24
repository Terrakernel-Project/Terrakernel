#ifndef FAT32_HPP
#define FAT32_HPP 1

#include <cstdint>

struct fat_file_info {
	char long_name[256];
	uint8_t short_name[11];
	uint32_t first_cluster;
	uint32_t size;
	uint8_t attributes;
	bool is_directory;
	uint32_t dir_cluster;
	uint32_t dir_entry_offset;
};

void fat_init();

bool fat_create_file(const char* path, bool is_directory);
bool fat_delete_file(const char* path);

int fat_read_file(const char* path, void* buffer, uint32_t size, uint32_t offset);
int fat_write_file(const char* path, const void* buffer, uint32_t size, uint32_t offset);

bool fat_stat_file(const char* path, fat_file_info* info);
bool fat_rename_file(const char* old_path, const char* new_path);
bool fat_move_file(const char* old_path, const char* new_path);

bool fat_stat_dir(const char* path, fat_file_info* info);
bool fat_make_dir(const char* path);
bool fat_remove_dir(const char* path);
bool fat_read_dir(const char* path, uint64_t* offset, uint64_t entries_count, void* __restrict buf);
bool fat_rename_dir(const char* old_path, const char* new_name);
bool fat_move_dir(const char* old_path, const char* new_path);

#endif
