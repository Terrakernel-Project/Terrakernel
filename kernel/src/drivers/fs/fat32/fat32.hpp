#ifndef FAT32_HPP
#define FAT32_HPP 1

#include <cstdint>
#include <cstddef>
#include <config.hpp>

#ifdef CONFIG_FAT_VERBOSE
#	define FDPRINTF(fmt, ...) printf("[ %s ] " fmt, __PRETTY_FUNCTION__, ##__VA_ARGS__)
#else
#	define FDPRINTF(fmt, ...)
#endif

struct __attribute__((packed)) fat32_bpb {
    uint8_t  jmp_boot[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
};

struct __attribute__((packed)) fat32_dir_entry {
    uint8_t  name[11];
    uint8_t  attr;
    uint8_t  nt_reserved;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t last_access_date;
    uint16_t first_cluster_hi;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_lo;
    uint32_t file_size;
};

struct __attribute__((packed)) fat32_lfn_entry {
    uint8_t  sequence;
    uint16_t name1[5];
    uint8_t  attr;
    uint8_t  type;
    uint8_t  checksum;
    uint16_t name2[6];
    uint16_t first_cluster_lo;
    uint16_t name3[2];
};

#define FAT32_ATTR_READ_ONLY  0x01
#define FAT32_ATTR_HIDDEN     0x02
#define FAT32_ATTR_SYSTEM     0x04
#define FAT32_ATTR_VOLUME_ID  0x08
#define FAT32_ATTR_DIRECTORY  0x10
#define FAT32_ATTR_ARCHIVE    0x20
#define FAT32_ATTR_LONG_NAME  0x0F

#define FAT32_CLUSTER_FREE    0x00000000
#define FAT32_CLUSTER_BAD     0x0FFFFFF7
#define FAT32_CLUSTER_EOC     0x0FFFFFF8
#define FAT32_CLUSTER_MASK    0x0FFFFFFF

enum class FATType { FAT12, FAT16, FAT32 };

struct fat32_state {
	fat32_bpb bpb;
	uint32_t fat_begin_lba;
	uint32_t cluster_begin_lba;
	uint32_t sectors_per_cluster;
	uint32_t bytes_per_cluster;
	uint32_t root_dir_cluster;
	uint32_t fat_size_sectors;
	int part_sn;
	uint8_t* fat_cache;
	uint32_t fat_cache_sector;
	bool fat_cache_valid;
};

void fat32_init();
bool fat32_read_boot_sector();
uint32_t fat32_get_next_cluster(uint32_t cluster);
uint32_t fat32_cluster_to_lba(uint32_t cluster);
bool fat32_read_cluster(uint32_t cluster, uint8_t* buffer);
bool fat32_write_cluster(uint32_t cluster, const uint8_t* buffer);

bool fat32_find_file_in_directory(uint32_t dir_cluster, const char* filename, 
                                   fat32_dir_entry* out_entry, uint32_t* out_cluster);
bool fat32_read_directory_entries(uint32_t cluster, fat32_dir_entry* entries, 
                                   size_t* count, size_t max_entries);
bool fat32_parse_lfn(const fat32_lfn_entry* lfn_entries, size_t count, 
                     char* output, size_t output_size);
uint8_t fat32_lfn_checksum(const uint8_t* short_name);

bool fat32_parse_path(const char* path, char* components[], size_t* count);
bool fat32_navigate_path(const char* path, uint32_t* out_cluster, bool* is_directory);

bool fat32_read_file(uint32_t first_cluster, uint32_t file_size, 
                     uint8_t* buffer, size_t buffer_size);
bool fat32_write_file(uint32_t first_cluster, const uint8_t* data, 
                      uint32_t data_size);

void fat32_filename_to_83(const char* filename, uint8_t* name_83);
bool fat32_match_filename(const uint8_t* name_83, const char* filename);
void fat32_83_to_filename(const uint8_t* name_83, char* filename);

uint32_t fat32_allocate_cluster();
bool fat32_free_cluster_chain(uint32_t first_cluster);
bool fat32_write_fat_entry(uint32_t cluster, uint32_t value);
bool fat32_extend_file(uint32_t* last_cluster, uint32_t clusters_needed);

bool fat32_find_free_entry(uint32_t dir_cluster, uint32_t* entry_cluster, 
                           uint32_t* entry_offset);
bool fat32_write_dir_entry(uint32_t cluster, uint32_t offset, 
                           const fat32_dir_entry* entry);
bool fat32_mark_entry_deleted(uint32_t dir_cluster, const char* filename);
bool fat32_update_dir_entry(uint32_t dir_cluster, const char* filename,
                            const fat32_dir_entry* new_entry);

bool fat32_create_file(uint32_t dir_cluster, const char* filename,
                       uint32_t* out_cluster);
bool fat32_create_directory(uint32_t parent_cluster, const char* dirname,
                            uint32_t* out_cluster);

#endif
