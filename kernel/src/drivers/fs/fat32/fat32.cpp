#include "fat32.hpp"
#include <mem/mem.hpp>
#include <drivers/blockio/diskgeneric.hpp>
#include <cstdio>
#include <config.hpp>
#include <cstring>

#define NAME_MAX 255

#ifdef CONFIG_FAT_VERBOSE
#	define FDPRINTF(fmt, ...) printf("[ %s ] " fmt, __PRETTY_FUNCTION__, ##__VA_ARGS__)
#else
#	define FDPRINTF(fmt, ...)
#endif

#pragma pack(push, 1)

struct fat_bpb {
	uint8_t  jmp_boot[3];
	uint8_t  oem_name[8];
	uint16_t bytes_per_sector;
	uint8_t  sectors_per_cluster;
	uint16_t reserved_sector_count;
	uint8_t  num_fats;
	uint16_t root_entry_count;
	uint16_t total_sectors_16;
	uint8_t  media;
	uint16_t fat_size_16;
	uint16_t sectors_per_track;
	uint16_t num_heads;
	uint32_t hidden_sectors;
	uint32_t total_sectors_32;
};

struct fat16_ebpb {
	uint8_t drive_number;
	uint8_t reserved1;
	uint8_t boot_signature;
	uint32_t volume_id;
	uint8_t volume_label[11];
	uint8_t fs_type[8];
};

struct fat32_ebpb {
	uint32_t fat_size_32;
	uint16_t ext_flags;
	uint16_t fs_version;
	uint32_t root_cluster;
	uint16_t fs_info;
	uint16_t backup_boot_sector;
	uint8_t reserved[12];
	uint8_t drive_number;
	uint8_t reserved1;
	uint8_t boot_signature;
	uint32_t volume_id;
	uint8_t volume_label[11];
	uint8_t fs_type[8];
};

struct fat16_boot_sector {
	fat_bpb bpb;
	fat16_ebpb ebpb;
	uint8_t boot_code[448];
	uint16_t signature;
};

struct fat32_boot_sector {
	fat_bpb bpb;
	fat32_ebpb ebpb;
	uint8_t boot_code[420];
	uint16_t signature;
};

struct fat_dir_entry {
    uint8_t  name[8];
    uint8_t  ext[3];
    uint8_t  attr;
    uint8_t  reserved;
    uint8_t  creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
};

struct fat_lfn_entry {
	uint8_t  seq;
	uint16_t name1[5];
	uint8_t  attr;
	uint8_t  type;
	uint8_t  checksum;
	uint16_t name2[6];
	uint16_t first_cluster_low;
	uint16_t name3[2];
};

#pragma pack(pop)

enum class FAT_TYPE {
	FAT32,
	FAT16,
};

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN    0x02
#define ATTR_SYSTEM    0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20
#define ATTR_LFN       0x0F
#define LFN_LAST       0x40

FAT_TYPE fat_type;
fat16_boot_sector* g_fat16_bs = nullptr;
fat32_boot_sector* g_fat32_bs = nullptr;

static uint8_t lfn_checksum(const uint8_t* short_name) {
	uint8_t sum = 0;
	for (int i = 0; i < 11; i++) {
		sum = ((sum & 1) << 7) + (sum >> 1) + short_name[i];
	}
	return sum;
}

static void utf16_to_utf8(const uint16_t* utf16, char* utf8, size_t max_len) {
	size_t out_idx = 0;
	for (size_t i = 0; utf16[i] != 0 && utf16[i] != 0xFFFF && out_idx < max_len - 1; i++) {
		if (utf16[i] < 0x80) {
			utf8[out_idx++] = (char)utf16[i];
		} else if (utf16[i] < 0x800) {
			if (out_idx + 1 < max_len - 1) {
				utf8[out_idx++] = 0xC0 | (utf16[i] >> 6);
				utf8[out_idx++] = 0x80 | (utf16[i] & 0x3F);
			}
		} else {
			if (out_idx + 2 < max_len - 1) {
				utf8[out_idx++] = 0xE0 | (utf16[i] >> 12);
				utf8[out_idx++] = 0x80 | ((utf16[i] >> 6) & 0x3F);
				utf8[out_idx++] = 0x80 | (utf16[i] & 0x3F);
			}
		}
	}
	utf8[out_idx] = '\0';
}

static void utf8_to_utf16(const char* utf8, uint16_t* utf16, size_t max_len) {
	size_t out_idx = 0;
	for (size_t i = 0; utf8[i] != '\0' && out_idx < max_len; ) {
		if ((utf8[i] & 0x80) == 0) {
			utf16[out_idx++] = utf8[i++];
		} else if ((utf8[i] & 0xE0) == 0xC0) {
			if (out_idx < max_len) {
				utf16[out_idx++] = ((utf8[i] & 0x1F) << 6) | (utf8[i+1] & 0x3F);
				i += 2;
			}
		} else if ((utf8[i] & 0xF0) == 0xE0) {
			if (out_idx < max_len) {
				utf16[out_idx++] = ((utf8[i] & 0x0F) << 12) | ((utf8[i+1] & 0x3F) << 6) | (utf8[i+2] & 0x3F);
				i += 3;
			}
		} else {
			i++;
		}
	}
	if (out_idx < max_len) {
		utf16[out_idx] = 0;
	}
}

static void generate_short_name(const char* long_name, uint8_t* short_name) {
	mem::memset(short_name, ' ', 11);
	const char* dot = nullptr;
	for (const char* p = long_name; *p; p++) {
		if (*p == '.') dot = p;
	}
	int name_idx = 0;
	for (const char* p = long_name; *p && (dot == nullptr || p < dot) && name_idx < 8; p++) {
		char c = *p;
		if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
		if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
			short_name[name_idx++] = c;
		}
	}
	if (name_idx < 8) {
		short_name[name_idx++] = '~';
		short_name[name_idx++] = '1';
	}
	if (dot) {
		int ext_idx = 0;
		for (const char* p = dot + 1; *p && ext_idx < 3; p++) {
			char c = *p;
			if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
			if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
				short_name[8 + ext_idx++] = c;
			}
		}
	}
}

static uint32_t fat_cluster_to_lba(uint32_t cluster) {
	uint32_t first_data_sector = 0;
	uint16_t reserved = 0;
	uint8_t  sectors_per_cluster = 0;
	uint8_t  num_fats = 0;
	uint32_t fat_size = 0;
	uint32_t root_dir_sectors = 0;
	if (fat_type == FAT_TYPE::FAT32) {
		reserved = g_fat32_bs->bpb.reserved_sector_count;
		sectors_per_cluster = g_fat32_bs->bpb.sectors_per_cluster;
		num_fats = g_fat32_bs->bpb.num_fats;
		fat_size = g_fat32_bs->ebpb.fat_size_32;
		root_dir_sectors = 0;
	} else {
		reserved = g_fat16_bs->bpb.reserved_sector_count;
		sectors_per_cluster = g_fat16_bs->bpb.sectors_per_cluster;
		num_fats = g_fat16_bs->bpb.num_fats;
		fat_size = g_fat16_bs->bpb.fat_size_16;
		root_dir_sectors = ((g_fat16_bs->bpb.root_entry_count * 32) + (g_fat16_bs->bpb.bytes_per_sector - 1))
							/ g_fat16_bs->bpb.bytes_per_sector;
	}
	first_data_sector = reserved + (num_fats * fat_size) + root_dir_sectors;
	return first_data_sector + (cluster - 2) * sectors_per_cluster;
}

static size_t fat_read_cluster(uint32_t cluster, uint8_t* buffer) {
	uint8_t sectors_per_cluster = (fat_type == FAT_TYPE::FAT32)
		? g_fat32_bs->bpb.sectors_per_cluster
		: g_fat16_bs->bpb.sectors_per_cluster;
	uint16_t bytes_per_sector = (fat_type == FAT_TYPE::FAT32)
		? g_fat32_bs->bpb.bytes_per_sector
		: g_fat16_bs->bpb.bytes_per_sector;
	uint32_t first_sector = fat_cluster_to_lba(cluster);
	size_t total_bytes = sectors_per_cluster * bytes_per_sector;
	drivers::blockio::diskgeneric::read(drivers::blockio::diskgeneric::get_boot_disk_sn(), drivers::blockio::diskgeneric::get_esp_part_sn(), first_sector, sectors_per_cluster, buffer, total_bytes);
	return total_bytes;
}

static size_t fat_write_cluster(uint32_t cluster, const uint8_t* buffer) {
	uint8_t sectors_per_cluster = (fat_type == FAT_TYPE::FAT32)
		? g_fat32_bs->bpb.sectors_per_cluster
		: g_fat16_bs->bpb.sectors_per_cluster;
	uint16_t bytes_per_sector = (fat_type == FAT_TYPE::FAT32)
		? g_fat32_bs->bpb.bytes_per_sector
		: g_fat16_bs->bpb.bytes_per_sector;
	uint32_t first_sector = fat_cluster_to_lba(cluster);
	size_t total_bytes = sectors_per_cluster * bytes_per_sector;
	drivers::blockio::diskgeneric::write(drivers::blockio::diskgeneric::get_boot_disk_sn(), drivers::blockio::diskgeneric::get_esp_part_sn(), first_sector, sectors_per_cluster, buffer, total_bytes);
	return total_bytes;
}

static uint32_t fat_read_fat_entry(uint32_t cluster) {
	uint32_t next = 0;
	uint16_t bytes_per_sector = (fat_type == FAT_TYPE::FAT32)
		? g_fat32_bs->bpb.bytes_per_sector
		: g_fat16_bs->bpb.bytes_per_sector;
	uint8_t* sector = (uint8_t*)mem::heap::malloc(bytes_per_sector);
	if (!sector) return 0;
	if (fat_type == FAT_TYPE::FAT32) {
		uint32_t fat_sector = g_fat32_bs->bpb.reserved_sector_count + (cluster * 4) / bytes_per_sector;
		uint32_t offset = (cluster * 4) % bytes_per_sector;
		drivers::blockio::diskgeneric::read(drivers::blockio::diskgeneric::get_boot_disk_sn(), drivers::blockio::diskgeneric::get_esp_part_sn(), fat_sector, 1, sector, bytes_per_sector);
		next = *(uint32_t*)(sector + offset);
		next &= 0x0FFFFFFF;
	} else {
		uint32_t fat_sector = g_fat16_bs->bpb.reserved_sector_count + (cluster * 2) / bytes_per_sector;
		uint32_t offset = (cluster * 2) % bytes_per_sector;
		drivers::blockio::diskgeneric::read(drivers::blockio::diskgeneric::get_boot_disk_sn(), drivers::blockio::diskgeneric::get_esp_part_sn(), fat_sector, 1, sector, bytes_per_sector);
		next = *(uint16_t*)(sector + offset);
	}
	mem::heap::free(sector);
	return next;
}

static void fat_write_fat_entry(uint32_t cluster, uint32_t value) {
	uint16_t bytes_per_sector = (fat_type == FAT_TYPE::FAT32)
		? g_fat32_bs->bpb.bytes_per_sector
		: g_fat16_bs->bpb.bytes_per_sector;
	uint8_t num_fats = (fat_type == FAT_TYPE::FAT32)
		? g_fat32_bs->bpb.num_fats
		: g_fat16_bs->bpb.num_fats;
	uint32_t fat_size = (fat_type == FAT_TYPE::FAT32)
		? g_fat32_bs->ebpb.fat_size_32
		: g_fat16_bs->bpb.fat_size_16;
	uint8_t* sector = (uint8_t*)mem::heap::malloc(bytes_per_sector);
	if (!sector) return;
	if (fat_type == FAT_TYPE::FAT32) {
		uint32_t fat_sector = g_fat32_bs->bpb.reserved_sector_count + (cluster * 4) / bytes_per_sector;
		uint32_t offset = (cluster * 4) % bytes_per_sector;
		for (uint8_t fat_num = 0; fat_num < num_fats; fat_num++) {
			uint32_t current_fat_sector = fat_sector + (fat_num * fat_size);
			drivers::blockio::diskgeneric::read(drivers::blockio::diskgeneric::get_boot_disk_sn(), drivers::blockio::diskgeneric::get_esp_part_sn(), current_fat_sector, 1, sector, bytes_per_sector);
			uint32_t* entry = (uint32_t*)(sector + offset);
			*entry = (*entry & 0xF0000000) | (value & 0x0FFFFFFF);
			drivers::blockio::diskgeneric::write(drivers::blockio::diskgeneric::get_boot_disk_sn(), drivers::blockio::diskgeneric::get_esp_part_sn(), current_fat_sector, 1, sector, bytes_per_sector);
		}
	} else {
		uint32_t fat_sector = g_fat16_bs->bpb.reserved_sector_count + (cluster * 2) / bytes_per_sector;
		uint32_t offset = (cluster * 2) % bytes_per_sector;
		for (uint8_t fat_num = 0; fat_num < num_fats; fat_num++) {
			uint32_t current_fat_sector = fat_sector + (fat_num * fat_size);
			drivers::blockio::diskgeneric::read(drivers::blockio::diskgeneric::get_boot_disk_sn(), drivers::blockio::diskgeneric::get_esp_part_sn(), current_fat_sector, 1, sector, bytes_per_sector);
			*(uint16_t*)(sector + offset) = (uint16_t)value;
			drivers::blockio::diskgeneric::write(drivers::blockio::diskgeneric::get_boot_disk_sn(), drivers::blockio::diskgeneric::get_esp_part_sn(), current_fat_sector, 1, sector, bytes_per_sector);
		}
	}
	mem::heap::free(sector);
}

static bool fat_is_eof(uint32_t cluster) {
	if (fat_type == FAT_TYPE::FAT32) {
		return cluster >= 0x0FFFFFF8;
	} else {
		return cluster >= 0xFFF8;
	}
}

static uint32_t fat_allocate_cluster() {
	uint32_t total_clusters;
	uint32_t start_cluster = 2;
	if (fat_type == FAT_TYPE::FAT32) {
		uint32_t total_sectors = g_fat32_bs->bpb.total_sectors_32;
		uint32_t data_sectors = total_sectors - (g_fat32_bs->bpb.reserved_sector_count + 
			(g_fat32_bs->bpb.num_fats * g_fat32_bs->ebpb.fat_size_32));
		total_clusters = data_sectors / g_fat32_bs->bpb.sectors_per_cluster;
	} else {
		uint32_t total_sectors = g_fat16_bs->bpb.total_sectors_16 ? g_fat16_bs->bpb.total_sectors_16 : g_fat16_bs->bpb.total_sectors_32;
		uint32_t root_dir_sectors = ((g_fat16_bs->bpb.root_entry_count * 32) + (g_fat16_bs->bpb.bytes_per_sector - 1)) / g_fat16_bs->bpb.bytes_per_sector;
		uint32_t data_sectors = total_sectors - (g_fat16_bs->bpb.reserved_sector_count + 
			(g_fat16_bs->bpb.num_fats * g_fat16_bs->bpb.fat_size_16) + root_dir_sectors);
		total_clusters = data_sectors / g_fat16_bs->bpb.sectors_per_cluster;
	}
	for (uint32_t cluster = start_cluster; cluster < total_clusters + 2; cluster++) {
		uint32_t entry = fat_read_fat_entry(cluster);
		if (entry == 0) {
			uint32_t eof_marker = (fat_type == FAT_TYPE::FAT32) ? 0x0FFFFFFF : 0xFFFF;
			fat_write_fat_entry(cluster, eof_marker);
			return cluster;
		}
	}
	return 0;
}

static void fat_free_cluster_chain(uint32_t first_cluster) {
	uint32_t cluster = first_cluster;
	while (cluster != 0 && !fat_is_eof(cluster)) {
		uint32_t next = fat_read_fat_entry(cluster);
		fat_write_fat_entry(cluster, 0);
		cluster = next;
	}
}

static uint32_t fat_get_last_cluster(uint32_t first_cluster) {
	uint32_t cluster = first_cluster;
	uint32_t prev = cluster;
	while (cluster != 0 && !fat_is_eof(cluster)) {
		prev = cluster;
		cluster = fat_read_fat_entry(cluster);
	}
	return prev;
}

static uint32_t fat_extend_cluster_chain(uint32_t last_cluster) {
	uint32_t new_cluster = fat_allocate_cluster();
	if (new_cluster == 0) return 0;
	fat_write_fat_entry(last_cluster, new_cluster);
	return new_cluster;
}

static uint32_t fat_get_root_cluster() {
	if (fat_type == FAT_TYPE::FAT32) {
		return g_fat32_bs->ebpb.root_cluster;
	}
	return 0;
}

static uint32_t fat_get_root_sector() {
	if (fat_type == FAT_TYPE::FAT16) {
		uint16_t reserved = g_fat16_bs->bpb.reserved_sector_count;
		uint8_t num_fats = g_fat16_bs->bpb.num_fats;
		uint16_t fat_size = g_fat16_bs->bpb.fat_size_16;
		return reserved + (num_fats * fat_size);
	}
	return 0;
}

static uint32_t fat_get_root_sectors() {
	if (fat_type == FAT_TYPE::FAT16) {
		return ((g_fat16_bs->bpb.root_entry_count * 32) + (g_fat16_bs->bpb.bytes_per_sector - 1)) / g_fat16_bs->bpb.bytes_per_sector;
	}
	return 0;
}

static bool fat_read_dir_entries(uint32_t dir_cluster, fat_dir_entry** entries_out, uint32_t* count_out, bool is_root) {
	uint16_t bytes_per_sector = (fat_type == FAT_TYPE::FAT32) ? g_fat32_bs->bpb.bytes_per_sector : g_fat16_bs->bpb.bytes_per_sector;
	uint8_t sectors_per_cluster = (fat_type == FAT_TYPE::FAT32) ? g_fat32_bs->bpb.sectors_per_cluster : g_fat16_bs->bpb.sectors_per_cluster;
	uint32_t total_size = 0;
	uint8_t* data = nullptr;
	if (is_root && fat_type == FAT_TYPE::FAT16) {
		uint32_t root_sectors = fat_get_root_sectors();
		total_size = root_sectors * bytes_per_sector;
		data = (uint8_t*)mem::heap::malloc(total_size);
		if (!data) return false;
		uint32_t root_sector = fat_get_root_sector();
		drivers::blockio::diskgeneric::read(drivers::blockio::diskgeneric::get_boot_disk_sn(), drivers::blockio::diskgeneric::get_esp_part_sn(), root_sector, root_sectors, data, total_size);
	} else {
		uint32_t cluster = dir_cluster;
		uint32_t num_clusters = 0;
		while (cluster != 0 && !fat_is_eof(cluster)) {
			num_clusters++;
			cluster = fat_read_fat_entry(cluster);
		}
		total_size = num_clusters * sectors_per_cluster * bytes_per_sector;
		data = (uint8_t*)mem::heap::malloc(total_size);
		if (!data) return false;
		cluster = dir_cluster;
		uint32_t offset = 0;
		while (cluster != 0 && !fat_is_eof(cluster)) {
			fat_read_cluster(cluster, data + offset);
			offset += sectors_per_cluster * bytes_per_sector;
			cluster = fat_read_fat_entry(cluster);
		}
	}
	*entries_out = (fat_dir_entry*)data;
	*count_out = total_size / sizeof(fat_dir_entry);
	return true;
}

static bool fat_write_dir_entries(uint32_t dir_cluster, fat_dir_entry* entries, uint32_t count, bool is_root) {
	uint16_t bytes_per_sector = (fat_type == FAT_TYPE::FAT32) ? g_fat32_bs->bpb.bytes_per_sector : g_fat16_bs->bpb.bytes_per_sector;
	uint8_t sectors_per_cluster = (fat_type == FAT_TYPE::FAT32) ? g_fat32_bs->bpb.sectors_per_cluster : g_fat16_bs->bpb.sectors_per_cluster;
	uint32_t total_size = count * sizeof(fat_dir_entry);
	uint8_t* data = (uint8_t*)entries;
	if (is_root && fat_type == FAT_TYPE::FAT16) {
		uint32_t root_sectors = fat_get_root_sectors();
		uint32_t root_sector = fat_get_root_sector();
		drivers::blockio::diskgeneric::write(drivers::blockio::diskgeneric::get_boot_disk_sn(), drivers::blockio::diskgeneric::get_esp_part_sn(), root_sector, root_sectors, data, total_size);
	} else {
		uint32_t cluster = dir_cluster;
		uint32_t offset = 0;
		uint32_t cluster_size = sectors_per_cluster * bytes_per_sector;
		while (cluster != 0 && !fat_is_eof(cluster) && offset < total_size) {
			fat_write_cluster(cluster, data + offset);
			offset += cluster_size;
			cluster = fat_read_fat_entry(cluster);
		}
	}
	return true;
}

static bool fat_find_file_in_dir(uint32_t dir_cluster, const char* filename, fat_file_info* info, bool is_root) {
	fat_dir_entry* entries = nullptr;
	uint32_t count = 0;
	FDPRINTF("Searching for '%s' in dir_cluster=%u, is_root=%d\n\r", filename, dir_cluster, is_root);
	if (!fat_read_dir_entries(dir_cluster, &entries, &count, is_root)) {
		FDPRINTF("Failed to read dir entries\n\r");
		return false;
	}
	FDPRINTF("Read %u directory entries\n\r", count);
	uint16_t lfn_buffer[256] = {0};
	int lfn_idx = 0;
	bool found = false;
	for (uint32_t i = 0; i < count; i++) {
		if (entries[i].name[0] == 0x00) {
			FDPRINTF("Entry %u: end of directory\n\r", i);
			break;
		}
		if (entries[i].name[0] == 0xE5) {
			FDPRINTF("Entry %u: deleted\n\r", i);
			continue;
		}
		if (entries[i].attr == ATTR_LFN) {
			FDPRINTF("Entry %u: LFN\n\r", i);
			fat_lfn_entry* lfn = (fat_lfn_entry*)&entries[i];
			int seq = lfn->seq & 0x1F;
			int base_idx = (seq - 1) * 13;
			for (int j = 0; j < 5; j++) lfn_buffer[base_idx + j] = lfn->name1[j];
			for (int j = 0; j < 6; j++) lfn_buffer[base_idx + 5 + j] = lfn->name2[j];
			for (int j = 0; j < 2; j++) lfn_buffer[base_idx + 11 + j] = lfn->name3[j];
			if (lfn->seq & LFN_LAST) {
				lfn_idx = base_idx + 13;
			}
		} else {
			char long_name[256] = {0};
			if (lfn_idx > 0) {
				utf16_to_utf8(lfn_buffer, long_name, 256);
				lfn_idx = 0;
				mem::memset(lfn_buffer, 0, sizeof(lfn_buffer));
			} else {
				int name_len = 0;
				for (int j = 0; j < 8 && entries[i].name[j] != ' '; j++) {
					long_name[name_len++] = entries[i].name[j];
				}
				if (entries[i].ext[0] != ' ') {
					long_name[name_len++] = '.';
					for (int j = 0; j < 3 && entries[i].ext[j] != ' '; j++) {
						long_name[name_len++] = entries[i].ext[j];
					}
				}
				long_name[name_len] = '\0';
			}
			FDPRINTF("Entry %u: name='%s', attr=0x%02X\n\r", i, long_name, entries[i].attr);
			bool name_match = false;
			if (strcmp(long_name, filename) == 0) {
				name_match = true;
				FDPRINTF("  -> Exact match!\n\r");
			} else {
				char short_name_str[13];
				int name_len = 0;
				for (int j = 0; j < 8 && entries[i].name[j] != ' '; j++) {
					short_name_str[name_len++] = entries[i].name[j];
				}
				if (entries[i].ext[0] != ' ') {
					short_name_str[name_len++] = '.';
					for (int j = 0; j < 3 && entries[i].ext[j] != ' '; j++) {
						short_name_str[name_len++] = entries[i].ext[j];
					}
				}
				short_name_str[name_len] = '\0';
				FDPRINTF("  -> Short name: '%s'\n\r", short_name_str);
				if (strcmp(short_name_str, filename) == 0) {
					name_match = true;
					FDPRINTF("  -> Short name match!\n\r");
				}
			}
			if (name_match) {
				strcpy(info->long_name, long_name);
				mem::memcpy(info->short_name, entries[i].name, 11);
				info->first_cluster = ((uint32_t)entries[i].first_cluster_high << 16) | entries[i].first_cluster_low;
				info->size = entries[i].file_size;
				info->attributes = entries[i].attr;
				info->is_directory = (entries[i].attr & ATTR_DIRECTORY) != 0;
				info->dir_cluster = dir_cluster;
				info->dir_entry_offset = i;
				found = true;
				FDPRINTF("FOUND! cluster=%u, size=%u\n\r", info->first_cluster, info->size);
				break;
			}
		}
	}
	mem::heap::free(entries);
	FDPRINTF("Search result: %s\n\r", found ? "FOUND" : "NOT FOUND");
	return found;
}

static bool fat_navigate_path(const char* path, fat_file_info* info) {
	if (!path || path[0] != '/') return false;
	path++;
	if (*path == '\0') {
		info->first_cluster = fat_get_root_cluster();
		info->is_directory = true;
		info->size = 0;
		info->dir_cluster = fat_type == FAT_TYPE::FAT16 ? 0 : fat_get_root_cluster();
		strcpy(info->long_name, "/");
		return true;
	}
	uint32_t current_cluster = fat_get_root_cluster();
	bool is_root = true;
	char component[256];
	const char* p = path;
	while (*p) {
		int comp_idx = 0;
		while (*p && *p != '/' && comp_idx < 255) {
			component[comp_idx++] = *p++;
		}
		component[comp_idx] = '\0';
		if (*p == '/') p++;
		if (comp_idx == 0) continue;
		FDPRINTF("Looking for component '%s' in cluster %u\n\r", component, current_cluster);
		fat_file_info temp_info;
		if (!fat_find_file_in_dir(current_cluster, component, &temp_info, is_root)) {
			FDPRINTF("Component '%s' not found\n\r", component);
			return false;
		}
		if (*p == '\0') {
			*info = temp_info;
			return true;
		}
		if (!temp_info.is_directory) {
			FDPRINTF("Component '%s' is not a directory\n\r", component);
			return false;
		}
		current_cluster = temp_info.first_cluster;
		is_root = false;
	}
	return false;
}

static bool fat_find_free_dir_entry(uint32_t dir_cluster, uint32_t* entry_idx, uint32_t num_entries_needed, bool is_root) {
	fat_dir_entry* entries = nullptr;
	uint32_t count = 0;
	if (!fat_read_dir_entries(dir_cluster, &entries, &count, is_root)) {
		return false;
	}
	uint32_t consecutive = 0;
	uint32_t start_idx = 0;
	for (uint32_t i = 0; i < count; i++) {
		if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
			if (consecutive == 0) start_idx = i;
			consecutive++;
			if (consecutive >= num_entries_needed) {
				*entry_idx = start_idx;
				mem::heap::free(entries);
				return true;
			}
		} else {
			consecutive = 0;
		}
	}
	mem::heap::free(entries);
	return false;
}

static bool fat_create_lfn_entries(const char* long_name, uint8_t* short_name, fat_dir_entry** lfn_entries_out, uint32_t* num_lfn_out) {
	size_t name_len = strlen(long_name);
	uint32_t num_lfn = (name_len + 12) / 13;
	if (num_lfn == 0 || num_lfn > 20) return false;
	fat_lfn_entry* lfn_entries = (fat_lfn_entry*)mem::heap::malloc(num_lfn * sizeof(fat_lfn_entry));
	if (!lfn_entries) return false;
	mem::memset(lfn_entries, 0xFF, num_lfn * sizeof(fat_lfn_entry));
	uint16_t utf16_name[256];
	utf8_to_utf16(long_name, utf16_name, 256);
	uint8_t checksum = lfn_checksum(short_name);
	for (uint32_t i = 0; i < num_lfn; i++) {
		uint32_t lfn_idx = num_lfn - 1 - i;
		fat_lfn_entry* lfn = &lfn_entries[i];
		lfn->seq = lfn_idx + 1;
		if (i == 0) lfn->seq |= LFN_LAST;
		lfn->attr = ATTR_LFN;
		lfn->type = 0;
		lfn->checksum = checksum;
		lfn->first_cluster_low = 0;
		int base = lfn_idx * 13;
		for (int j = 0; j < 5; j++) {
			lfn->name1[j] = (base + j < (int)name_len) ? utf16_name[base + j] : (base + j == (int)name_len ? 0 : 0xFFFF);
		}
		for (int j = 0; j < 6; j++) {
			lfn->name2[j] = (base + 5 + j < (int)name_len) ? utf16_name[base + 5 + j] : (base + 5 + j == (int)name_len ? 0 : 0xFFFF);
		}
		for (int j = 0; j < 2; j++) {
			lfn->name3[j] = (base + 11 + j < (int)name_len) ? utf16_name[base + 11 + j] : (base + 11 + j == (int)name_len ? 0 : 0xFFFF);
		}
	}
	*lfn_entries_out = (fat_dir_entry*)lfn_entries;
	*num_lfn_out = num_lfn;
	return true;
}

bool fat_create_file(const char* path, bool is_directory) {
	if (!path || path[0] != '/') return false;
	char parent_path[256];
	char filename[256];
	const char* last_slash = nullptr;
	for (const char* p = path; *p; p++) {
		if (*p == '/') last_slash = p;
	}
	if (last_slash == path) {
		parent_path[0] = '/';
		parent_path[1] = '\0';
		strcpy(filename, path + 1);
	} else if (last_slash) {
		size_t len = last_slash - path;
		mem::memcpy(parent_path, path, len);
		parent_path[len] = '\0';
		strcpy(filename, last_slash + 1);
	} else {
		return false;
	}
	fat_file_info parent_info;
	if (!fat_navigate_path(parent_path, &parent_info)) {
		return false;
	}
	if (!parent_info.is_directory) return false;
	uint32_t parent_cluster = parent_info.first_cluster;
	bool parent_is_root = (fat_type == FAT_TYPE::FAT16 && parent_cluster == 0);
	uint8_t short_name[11];
	generate_short_name(filename, short_name);
	fat_dir_entry* lfn_entries = nullptr;
	uint32_t num_lfn = 0;
	bool use_lfn = strlen(filename) > 12;
	if (use_lfn) {
		if (!fat_create_lfn_entries(filename, short_name, &lfn_entries, &num_lfn)) {
			return false;
		}
	}
	uint32_t num_entries_needed = (use_lfn ? num_lfn : 0) + 1;
	uint32_t entry_idx = 0;
	if (!fat_find_free_dir_entry(parent_cluster, &entry_idx, num_entries_needed, parent_is_root)) {
		if (lfn_entries) mem::heap::free(lfn_entries);
		return false;
	}
	fat_dir_entry* entries = nullptr;
	uint32_t count = 0;
	if (!fat_read_dir_entries(parent_cluster, &entries, &count, parent_is_root)) {
		if (lfn_entries) mem::heap::free(lfn_entries);
		return false;
	}
	if (use_lfn) {
		for (uint32_t i = 0; i < num_lfn; i++) {
			entries[entry_idx + i] = lfn_entries[i];
		}
		mem::heap::free(lfn_entries);
	}
	fat_dir_entry* new_entry = &entries[entry_idx + (use_lfn ? num_lfn : 0)];
	mem::memset(new_entry, 0, sizeof(fat_dir_entry));
	mem::memcpy(new_entry->name, short_name, 11);
	if (is_directory) {
		new_entry->attr = ATTR_DIRECTORY;
		uint32_t new_cluster = fat_allocate_cluster();
		if (new_cluster == 0) {
			mem::heap::free(entries);
			return false;
		}
		new_entry->first_cluster_high = (new_cluster >> 16) & 0xFFFF;
		new_entry->first_cluster_low = new_cluster & 0xFFFF;
		new_entry->file_size = 0;
		uint16_t bytes_per_sector = (fat_type == FAT_TYPE::FAT32) ? g_fat32_bs->bpb.bytes_per_sector : g_fat16_bs->bpb.bytes_per_sector;
		uint8_t sectors_per_cluster = (fat_type == FAT_TYPE::FAT32) ? g_fat32_bs->bpb.sectors_per_cluster : g_fat16_bs->bpb.sectors_per_cluster;
		uint32_t cluster_size = bytes_per_sector * sectors_per_cluster;
		uint8_t* dir_data = (uint8_t*)mem::heap::malloc(cluster_size);
		mem::memset(dir_data, 0, cluster_size);
		fat_dir_entry* dot_entry = (fat_dir_entry*)dir_data;
		mem::memset(dot_entry, 0, sizeof(fat_dir_entry));
		mem::memset(dot_entry->name, ' ', 11);
		dot_entry->name[0] = '.';
		dot_entry->attr = ATTR_DIRECTORY;
		dot_entry->first_cluster_high = (new_cluster >> 16) & 0xFFFF;
		dot_entry->first_cluster_low = new_cluster & 0xFFFF;
		fat_dir_entry* dotdot_entry = (fat_dir_entry*)(dir_data + sizeof(fat_dir_entry));
		mem::memset(dotdot_entry, 0, sizeof(fat_dir_entry));
		mem::memset(dotdot_entry->name, ' ', 11);
		dotdot_entry->name[0] = '.';
		dotdot_entry->name[1] = '.';
		dotdot_entry->attr = ATTR_DIRECTORY;
		dotdot_entry->first_cluster_high = (parent_cluster >> 16) & 0xFFFF;
		dotdot_entry->first_cluster_low = parent_cluster & 0xFFFF;
		fat_write_cluster(new_cluster, dir_data);
		mem::heap::free(dir_data);
	} else {
		new_entry->attr = ATTR_ARCHIVE;
		new_entry->first_cluster_high = 0;
		new_entry->first_cluster_low = 0;
		new_entry->file_size = 0;
	}
	fat_write_dir_entries(parent_cluster, entries, count, parent_is_root);
	mem::heap::free(entries);
	return true;
}

bool fat_delete_file(const char* path) {
	if (!path || path[0] != '/') return false;
	fat_file_info file_info;
	if (!fat_navigate_path(path, &file_info)) {
		return false;
	}
	if (file_info.first_cluster != 0) {
		fat_free_cluster_chain(file_info.first_cluster);
	}
	fat_file_info parent_info;
	char parent_path[256];
	const char* last_slash = nullptr;
	for (const char* p = path; *p; p++) {
		if (*p == '/') last_slash = p;
	}
	if (last_slash && last_slash != path) {
		size_t len = last_slash - path;
		mem::memcpy(parent_path, path, len);
		parent_path[len] = '\0';
	} else {
		parent_path[0] = '/';
		parent_path[1] = '\0';
	}
	if (!fat_navigate_path(parent_path, &parent_info)) return false;
	bool is_root = (parent_info.first_cluster == 0 && fat_type == FAT_TYPE::FAT16);
	fat_dir_entry* entries = nullptr;
	uint32_t count = 0;
	if (!fat_read_dir_entries(parent_info.first_cluster, &entries, &count, is_root)) {
		return false;
	}
	uint32_t lfn_start = file_info.dir_entry_offset;
	while (lfn_start > 0 && entries[lfn_start - 1].attr == ATTR_LFN) {
		lfn_start--;
	}
	for (uint32_t i = lfn_start; i <= file_info.dir_entry_offset; i++) {
		entries[i].name[0] = 0xE5;
	}
	fat_write_dir_entries(parent_info.first_cluster, entries, count, is_root);
	mem::heap::free(entries);
	return true;
}

int fat_read_file(const char* path, void* buffer, uint32_t size, uint32_t offset) {
	FDPRINTF("Reading file '%s', size=%u, offset=%u\n\r", path, size, offset);
	if (!path || !buffer || path[0] != '/') {
		FDPRINTF("Invalid parameters\n\r");
		return -1;
	}
	fat_file_info file_info;
	if (!fat_navigate_path(path, &file_info)) {
		FDPRINTF("File not found\n\r");
		return -1;
	}
	FDPRINTF("File found: cluster=%u, size=%u\n\r", file_info.first_cluster, file_info.size);
	if (file_info.is_directory) {
		FDPRINTF("Is a directory\n\r");
		return -1;
	}
	if (offset >= file_info.size) {
		FDPRINTF("Offset beyond file size\n\r");
		return 0;
	}
	uint32_t to_read = size;
	if (offset + to_read > file_info.size) {
		to_read = file_info.size - offset;
	}
	FDPRINTF("Will read %u bytes\n\r", to_read);
	uint16_t bytes_per_sector = (fat_type == FAT_TYPE::FAT32) ? g_fat32_bs->bpb.bytes_per_sector : g_fat16_bs->bpb.bytes_per_sector;
	uint8_t sectors_per_cluster = (fat_type == FAT_TYPE::FAT32) ? g_fat32_bs->bpb.sectors_per_cluster : g_fat16_bs->bpb.sectors_per_cluster;
	uint32_t cluster_size = bytes_per_sector * sectors_per_cluster;
	FDPRINTF("cluster_size=%u bytes\n\r", cluster_size);
	uint32_t cluster_idx = offset / cluster_size;
	uint32_t cluster_offset = offset % cluster_size;
	uint32_t cluster = file_info.first_cluster;
	for (uint32_t i = 0; i < cluster_idx && cluster != 0 && !fat_is_eof(cluster); i++) {
		cluster = fat_read_fat_entry(cluster);
	}
	FDPRINTF("Starting read at cluster=%u, offset=%u\n\r", cluster, cluster_offset);
	if (cluster == 0 || fat_is_eof(cluster)) {
		FDPRINTF("Invalid starting cluster\n\r");
		return -1;
	}
	uint32_t bytes_read = 0;
	uint8_t* dest = (uint8_t*)buffer;
	while (to_read > 0 && cluster != 0 && !fat_is_eof(cluster)) {
		uint8_t* cluster_data = (uint8_t*)mem::heap::malloc(cluster_size);
		if (!cluster_data) {
			FDPRINTF("Failed to allocate cluster buffer\n\r");
			break;
		}
		fat_read_cluster(cluster, cluster_data);
		uint32_t bytes_to_copy = cluster_size - cluster_offset;
		if (bytes_to_copy > to_read) bytes_to_copy = to_read;
		FDPRINTF("Copying %u bytes from cluster %u\n\r", bytes_to_copy, cluster);
		mem::memcpy(dest, cluster_data + cluster_offset, bytes_to_copy);
		mem::heap::free(cluster_data);
		dest += bytes_to_copy;
		bytes_read += bytes_to_copy;
		to_read -= bytes_to_copy;
		cluster_offset = 0;
		cluster = fat_read_fat_entry(cluster);
	}
	FDPRINTF("Read complete: %u bytes\n\r", bytes_read);
	return bytes_read;
}

int fat_write_file(const char* path, const void* buffer, uint32_t size, uint32_t offset) {
	if (!path || !buffer || path[0] != '/') return -1;
	fat_file_info file_info;
	if (!fat_navigate_path(path, &file_info)) {
		return -1;
	}
	if (file_info.is_directory) return -1;
	uint16_t bytes_per_sector = (fat_type == FAT_TYPE::FAT32) ? g_fat32_bs->bpb.bytes_per_sector : g_fat16_bs->bpb.bytes_per_sector;
	uint8_t sectors_per_cluster = (fat_type == FAT_TYPE::FAT32) ? g_fat32_bs->bpb.sectors_per_cluster : g_fat16_bs->bpb.sectors_per_cluster;
	uint32_t cluster_size = bytes_per_sector * sectors_per_cluster;
	uint32_t needed_size = offset + size;
	uint32_t needed_clusters = (needed_size + cluster_size - 1) / cluster_size;
	if (file_info.first_cluster == 0) {
		file_info.first_cluster = fat_allocate_cluster();
		if (file_info.first_cluster == 0) return -1;
		fat_file_info parent_info;
		char parent_path[256];
		const char* last_slash = nullptr;
		for (const char* p = path; *p; p++) {
			if (*p == '/') last_slash = p;
		}
		if (last_slash && last_slash != path) {
			size_t len = last_slash - path;
			mem::memcpy(parent_path, path, len);
			parent_path[len] = '\0';
		} else {
			parent_path[0] = '/';
			parent_path[1] = '\0';
		}
		if (!fat_navigate_path(parent_path, &parent_info)) return -1;
		fat_dir_entry* entries = nullptr;
		uint32_t count = 0;
		bool is_root = (parent_info.first_cluster == 0 && fat_type == FAT_TYPE::FAT16);
		if (!fat_read_dir_entries(parent_info.first_cluster, &entries, &count, is_root)) {
			return -1;
		}
		entries[file_info.dir_entry_offset].first_cluster_high = (file_info.first_cluster >> 16) & 0xFFFF;
		entries[file_info.dir_entry_offset].first_cluster_low = file_info.first_cluster & 0xFFFF;
		fat_write_dir_entries(parent_info.first_cluster, entries, count, is_root);
		mem::heap::free(entries);
	}
	uint32_t current_clusters = 0;
	uint32_t cluster = file_info.first_cluster;
	uint32_t last_cluster = cluster;
	while (cluster != 0 && !fat_is_eof(cluster)) {
		current_clusters++;
		last_cluster = cluster;
		cluster = fat_read_fat_entry(cluster);
	}
	while (current_clusters < needed_clusters) {
		uint32_t new_cluster = fat_extend_cluster_chain(last_cluster);
		if (new_cluster == 0) break;
		last_cluster = new_cluster;
		current_clusters++;
	}
	uint32_t cluster_idx = offset / cluster_size;
	uint32_t cluster_offset = offset % cluster_size;
	cluster = file_info.first_cluster;
	for (uint32_t i = 0; i < cluster_idx && cluster != 0 && !fat_is_eof(cluster); i++) {
		cluster = fat_read_fat_entry(cluster);
	}
	if (cluster == 0 || fat_is_eof(cluster)) return -1;
	uint32_t bytes_written = 0;
	const uint8_t* src = (const uint8_t*)buffer;
	while (size > 0 && cluster != 0 && !fat_is_eof(cluster)) {
		uint8_t* cluster_data = (uint8_t*)mem::heap::malloc(cluster_size);
		if (!cluster_data) break;
		if (cluster_offset > 0 || size < cluster_size) {
			fat_read_cluster(cluster, cluster_data);
		}
		uint32_t bytes_to_copy = cluster_size - cluster_offset;
		if (bytes_to_copy > size) bytes_to_copy = size;
		mem::memcpy(cluster_data + cluster_offset, src, bytes_to_copy);
		fat_write_cluster(cluster, cluster_data);
		mem::heap::free(cluster_data);
		src += bytes_to_copy;
		bytes_written += bytes_to_copy;
		size -= bytes_to_copy;
		cluster_offset = 0;
		cluster = fat_read_fat_entry(cluster);
	}
	if (offset + bytes_written > file_info.size) {
		fat_file_info parent_info;
		char parent_path[256];
		const char* last_slash = nullptr;
		for (const char* p = path; *p; p++) {
			if (*p == '/') last_slash = p;
		}
		if (last_slash && last_slash != path) {
			size_t len = last_slash - path;
			mem::memcpy(parent_path, path, len);
			parent_path[len] = '\0';
		} else {
			parent_path[0] = '/';
			parent_path[1] = '\0';
		}
		if (fat_navigate_path(parent_path, &parent_info)) {
			fat_dir_entry* entries = nullptr;
			uint32_t count = 0;
			bool is_root = (parent_info.first_cluster == 0 && fat_type == FAT_TYPE::FAT16);
			if (fat_read_dir_entries(parent_info.first_cluster, &entries, &count, is_root)) {
				entries[file_info.dir_entry_offset].file_size = offset + bytes_written;
				fat_write_dir_entries(parent_info.first_cluster, entries, count, is_root);
				mem::heap::free(entries);
			}
		}
	}
	return bytes_written;
}

bool fat_stat_file(const char* path, fat_file_info* info) {
	if (!path || !info || path[0] != '/') return false;
	return fat_navigate_path(path, info);
}

bool fat_rename_file(const char* old_path, const char* new_path) {
	if (!old_path || !new_path || old_path[0] != '/' || new_path[0] != '/') return false;
	fat_file_info old_info;
	if (!fat_navigate_path(old_path, &old_info)) return false;
	char new_filename[256];
	const char* last_slash = nullptr;
	for (const char* p = new_path; *p; p++) {
		if (*p == '/') last_slash = p;
	}
	if (last_slash) {
		strcpy(new_filename, last_slash + 1);
	} else {
		return false;
	}
	uint32_t parent_cluster = old_info.dir_cluster;
	bool is_root = (fat_type == FAT_TYPE::FAT16 && parent_cluster == 0);
	fat_dir_entry* entries = nullptr;
	uint32_t count = 0;
	if (!fat_read_dir_entries(parent_cluster, &entries, &count, is_root)) {
		return false;
	}
	uint32_t lfn_start = old_info.dir_entry_offset;
	while (lfn_start > 0 && entries[lfn_start - 1].attr == ATTR_LFN) {
		lfn_start--;
	}
	for (uint32_t i = lfn_start; i <= old_info.dir_entry_offset; i++) {
		entries[i].name[0] = 0xE5;
	}
	uint8_t short_name[11];
	generate_short_name(new_filename, short_name);
	fat_dir_entry* lfn_entries = nullptr;
	uint32_t num_lfn = 0;
	bool use_lfn = strlen(new_filename) > 12;
	if (use_lfn) {
		if (!fat_create_lfn_entries(new_filename, short_name, &lfn_entries, &num_lfn)) {
			mem::heap::free(entries);
			return false;
		}
	}
	uint32_t num_entries_needed = (use_lfn ? num_lfn : 0) + 1;
	uint32_t entry_idx = 0;
	if (!fat_find_free_dir_entry(parent_cluster, &entry_idx, num_entries_needed, is_root)) {
		if (lfn_entries) mem::heap::free(lfn_entries);
		mem::heap::free(entries);
		return false;
	}
	if (use_lfn) {
		for (uint32_t i = 0; i < num_lfn; i++) {
			entries[entry_idx + i] = lfn_entries[i];
		}
		mem::heap::free(lfn_entries);
	}
	fat_dir_entry* new_entry = &entries[entry_idx + (use_lfn ? num_lfn : 0)];
	mem::memset(new_entry, 0, sizeof(fat_dir_entry));
	mem::memcpy(new_entry->name, short_name, 11);
	new_entry->attr = old_info.attributes;
	new_entry->first_cluster_high = (old_info.first_cluster >> 16) & 0xFFFF;
	new_entry->first_cluster_low = old_info.first_cluster & 0xFFFF;
	new_entry->file_size = old_info.size;
	fat_write_dir_entries(parent_cluster, entries, count, is_root);
	mem::heap::free(entries);
	return true;
}

bool fat_move_file(const char* old_path, const char* new_path) {
	if (!old_path || !new_path || old_path[0] != '/' || new_path[0] != '/') return false;
	fat_file_info file_info;
	if (!fat_navigate_path(old_path, &file_info)) {
		return false;
	}
	char new_filename[256];
	const char* last_slash = nullptr;
	for (const char* p = new_path; *p; p++) {
		if (*p == '/') last_slash = p;
	}
	if (last_slash) {
		strcpy(new_filename, last_slash + 1);
	} else {
		return false;
	}
	char old_parent_path[256];
	const char* old_slash = nullptr;
	for (const char* p = old_path; *p; p++) {
		if (*p == '/') old_slash = p;
	}
	if (old_slash && old_slash != old_path) {
		size_t len = old_slash - old_path;
		mem::memcpy(old_parent_path, old_path, len);
		old_parent_path[len] = '\0';
	} else {
		old_parent_path[0] = '/';
		old_parent_path[1] = '\0';
	}
	fat_file_info old_parent_info;
	if (!fat_navigate_path(old_parent_path, &old_parent_info)) return false;
	bool old_is_root = (old_parent_info.first_cluster == 0 && fat_type == FAT_TYPE::FAT16);
	char new_parent_path[256];
	if (last_slash && last_slash != new_path) {
		size_t len = last_slash - new_path;
		mem::memcpy(new_parent_path, new_path, len);
		new_parent_path[len] = '\0';
	} else {
		new_parent_path[0] = '/';
		new_parent_path[1] = '\0';
	}
	fat_file_info new_parent_info;
	if (!fat_navigate_path(new_parent_path, &new_parent_info)) return false;
	bool new_is_root = (new_parent_info.first_cluster == 0 && fat_type == FAT_TYPE::FAT16);
	fat_dir_entry* old_entries = nullptr;
	uint32_t old_count = 0;
	if (!fat_read_dir_entries(old_parent_info.first_cluster, &old_entries, &old_count, old_is_root)) {
		return false;
	}
	uint32_t lfn_start = file_info.dir_entry_offset;
	while (lfn_start > 0 && old_entries[lfn_start - 1].attr == ATTR_LFN) {
		lfn_start--;
	}
	for (uint32_t i = lfn_start; i <= file_info.dir_entry_offset; i++) {
		old_entries[i].name[0] = 0xE5;
	}
	fat_write_dir_entries(old_parent_info.first_cluster, old_entries, old_count, old_is_root);
	mem::heap::free(old_entries);
	uint8_t short_name[11];
	generate_short_name(new_filename, short_name);
	fat_dir_entry* lfn_entries = nullptr;
	uint32_t num_lfn = 0;
	bool use_lfn = strlen(new_filename) > 12;
	if (use_lfn) {
		if (!fat_create_lfn_entries(new_filename, short_name, &lfn_entries, &num_lfn)) {
			return false;
		}
	}
	uint32_t num_entries_needed = (use_lfn ? num_lfn : 0) + 1;
	uint32_t entry_idx = 0;
	if (!fat_find_free_dir_entry(new_parent_info.first_cluster, &entry_idx, num_entries_needed, new_is_root)) {
		if (lfn_entries) mem::heap::free(lfn_entries);
		return false;
	}
	fat_dir_entry* new_entries = nullptr;
	uint32_t new_count = 0;
	if (!fat_read_dir_entries(new_parent_info.first_cluster, &new_entries, &new_count, new_is_root)) {
		if (lfn_entries) mem::heap::free(lfn_entries);
		return false;
	}
	if (use_lfn) {
		for (uint32_t i = 0; i < num_lfn; i++) {
			new_entries[entry_idx + i] = lfn_entries[i];
		}
		mem::heap::free(lfn_entries);
	}
	fat_dir_entry* new_entry = &new_entries[entry_idx + (use_lfn ? num_lfn : 0)];
	mem::memset(new_entry, 0, sizeof(fat_dir_entry));
	mem::memcpy(new_entry->name, short_name, 11);
	new_entry->attr = file_info.attributes;
	new_entry->first_cluster_high = (file_info.first_cluster >> 16) & 0xFFFF;
	new_entry->first_cluster_low = file_info.first_cluster & 0xFFFF;
	new_entry->file_size = file_info.size;
	fat_write_dir_entries(new_parent_info.first_cluster, new_entries, new_count, new_is_root);
	mem::heap::free(new_entries);
	return true;
}

bool fat_stat_dir(const char* path, fat_file_info* info) {
    if (!path || !info || path[0] != '/') return false;
    if (!fat_navigate_path(path, info)) return false;
    if (!info->is_directory) return false;
    return true;
}

bool fat_make_dir(const char* path) {
    return fat_create_file(path, true);
}

bool fat_remove_dir(const char* path) {
    if (!path || path[0] != '/') return false;
    
    fat_file_info dir_info;
    if (!fat_navigate_path(path, &dir_info) || !dir_info.is_directory) {
        return false;
    }
    
    if (dir_info.first_cluster == 0 && fat_type == FAT_TYPE::FAT16) {
        return false;
    }
    
    fat_dir_entry* entries = nullptr;
    uint32_t count = 0;
    bool is_root_dir = false;
    if (!fat_read_dir_entries(dir_info.first_cluster, &entries, &count, false)) {
        return false;
    }
    
    bool empty = true;
    for (uint32_t i = 0; i < count; i++) {
        if (entries[i].name[0] == 0x00) break;
        if (entries[i].name[0] == 0xE5) continue;
        if (entries[i].attr == ATTR_LFN) continue;
        
        if (entries[i].name[0] == '.' && 
            (entries[i].name[1] == ' ' || (entries[i].name[1] == '.' && entries[i].name[2] == ' '))) {
            continue;
        }
        empty = false;
        break;
    }
    mem::heap::free(entries);
    if (!empty) return false;
    
    if (dir_info.first_cluster != 0) {
        fat_free_cluster_chain(dir_info.first_cluster);
    }
    
    fat_file_info parent_info;
    char parent_path[256];
    const char* last_slash = nullptr;
    for (const char* p = path; *p; p++) {
        if (*p == '/') last_slash = p;
    }
    if (last_slash && last_slash != path) {
        size_t len = last_slash - path;
        mem::memcpy(parent_path, path, len);
        parent_path[len] = '\0';
    } else {
        parent_path[0] = '/';
        parent_path[1] = '\0';
    }
    if (!fat_navigate_path(parent_path, &parent_info)) return false;
    
    bool parent_is_root = (parent_info.first_cluster == 0 && fat_type == FAT_TYPE::FAT16);
    fat_dir_entry* parent_entries = nullptr;
    uint32_t parent_count = 0;
    if (!fat_read_dir_entries(parent_info.first_cluster, &parent_entries, &parent_count, parent_is_root)) {
        return false;
    }
    
    uint32_t lfn_start = dir_info.dir_entry_offset;
    while (lfn_start > 0 && parent_entries[lfn_start - 1].attr == ATTR_LFN) {
        lfn_start--;
    }
    for (uint32_t i = lfn_start; i <= dir_info.dir_entry_offset; i++) {
        parent_entries[i].name[0] = 0xE5;
    }
    
    fat_write_dir_entries(parent_info.first_cluster, parent_entries, parent_count, parent_is_root);
    mem::heap::free(parent_entries);
    return true;
}

bool fat_read_dir(const char* path, uint64_t* offset, uint64_t entries_count, void* __restrict buf) {
    if (!path || !offset || !buf || path[0] != '/') return false;
    
    fat_file_info dir_info;
    if (!fat_navigate_path(path, &dir_info) || !dir_info.is_directory) {
        return false;
    }
    
    bool is_root = false;
    if (fat_type == FAT_TYPE::FAT16 && dir_info.first_cluster == 0) {
        is_root = true;
    }
    
    fat_dir_entry* raw_entries = nullptr;
    uint32_t raw_count = 0;
    if (!fat_read_dir_entries(dir_info.first_cluster, &raw_entries, &raw_count, is_root)) {
        return false;
    }
    
    uint64_t out_count = 0;
    uint64_t skipped = 0;
    char* out_buf = (char*)buf + sizeof(uint64_t);
    
    uint16_t lfn_buffer[256] = {0};
    int lfn_idx = 0;
    
    for (uint32_t i = 0; i < raw_count; i++) {
        if (raw_entries[i].name[0] == 0x00) break;
        
        if (raw_entries[i].name[0] == 0xE5) {
            lfn_idx = 0;
            continue;
        }
        
        if (raw_entries[i].attr == ATTR_LFN) {
            fat_lfn_entry* lfn = (fat_lfn_entry*)&raw_entries[i];
            int seq = lfn->seq & 0x1F;
            int base_idx = (seq - 1) * 13;
            for (int j = 0; j < 5; j++) lfn_buffer[base_idx + j] = lfn->name1[j];
            for (int j = 0; j < 6; j++) lfn_buffer[base_idx + 5 + j] = lfn->name2[j];
            for (int j = 0; j < 2; j++) lfn_buffer[base_idx + 11 + j] = lfn->name3[j];
            if (lfn->seq & LFN_LAST) {
                lfn_idx = base_idx + 13;
            }
            continue;
        }
        
        char name[NAME_MAX + 1] = {0};
        
        if (lfn_idx > 0) {
            utf16_to_utf8(lfn_buffer, name, NAME_MAX + 1);
            lfn_idx = 0;
            mem::memset(lfn_buffer, 0, sizeof(lfn_buffer));
        } else {
            int pos = 0;
            for (int j = 0; j < 8 && raw_entries[i].name[j] != ' '; j++) {
                name[pos++] = raw_entries[i].name[j];
            }
            if (raw_entries[i].ext[0] != ' ') {
                name[pos++] = '.';
                for (int j = 0; j < 3 && raw_entries[i].ext[j] != ' '; j++) {
                    name[pos++] = raw_entries[i].ext[j];
                }
            }
            name[pos] = '\0';
        }
        
        if (skipped < *offset) {
            skipped++;
            continue;
        }
        
        if (out_count < entries_count) {
            strncpy(out_buf + out_count * (NAME_MAX + 1), name, NAME_MAX);
            out_count++;
        } else {
            break;
        }
    }
    
    mem::heap::free(raw_entries);
    
    *offset += out_count;
    
    *(uint64_t*)buf = out_count;
    
    return true;
}

bool fat_rename_dir(const char* old_path, const char* new_name) {
    if (!old_path || !new_name || old_path[0] != '/') return false;
    
    fat_file_info dir_info;
    if (!fat_navigate_path(old_path, &dir_info) || !dir_info.is_directory) {
        return false;
    }

    fat_file_info parent_info;
    char parent_path[256];
    const char* last_slash = nullptr;
    for (const char* p = old_path; *p; p++) {
        if (*p == '/') last_slash = p;
    }
    if (last_slash && last_slash != old_path) {
        size_t len = last_slash - old_path;
        mem::memcpy(parent_path, old_path, len);
        parent_path[len] = '\0';
    } else {
        parent_path[0] = '/';
        parent_path[1] = '\0';
    }
    if (!fat_navigate_path(parent_path, &parent_info)) return false;
    
    bool parent_is_root = (parent_info.first_cluster == 0 && fat_type == FAT_TYPE::FAT16);
    
    fat_dir_entry* parent_entries = nullptr;
    uint32_t parent_count = 0;
    if (!fat_read_dir_entries(parent_info.first_cluster, &parent_entries, &parent_count, parent_is_root)) {
        return false;
    }
    
    uint32_t lfn_start = dir_info.dir_entry_offset;
    while (lfn_start > 0 && parent_entries[lfn_start - 1].attr == ATTR_LFN) {
        lfn_start--;
    }
    for (uint32_t i = lfn_start; i <= dir_info.dir_entry_offset; i++) {
        parent_entries[i].name[0] = 0xE5;
    }
    
    uint8_t short_name[11];
    generate_short_name(new_name, short_name);
    fat_dir_entry* lfn_entries = nullptr;
    uint32_t num_lfn = 0;
    bool use_lfn = strlen(new_name) > 12;
    if (use_lfn) {
        if (!fat_create_lfn_entries(new_name, short_name, &lfn_entries, &num_lfn)) {
            mem::heap::free(parent_entries);
            return false;
        }
    }
    
    uint32_t entry_idx = 0;
    uint32_t num_needed = (use_lfn ? num_lfn : 0) + 1;
    if (!fat_find_free_dir_entry(parent_info.first_cluster, &entry_idx, num_needed, parent_is_root)) {
        if (lfn_entries) mem::heap::free(lfn_entries);
        mem::heap::free(parent_entries);
        return false;
    }
    
    if (use_lfn) {
        for (uint32_t i = 0; i < num_lfn; i++) {
            parent_entries[entry_idx + i] = lfn_entries[i];
        }
        mem::heap::free(lfn_entries);
    }
    
    fat_dir_entry* new_entry = &parent_entries[entry_idx + (use_lfn ? num_lfn : 0)];
    mem::memset(new_entry, 0, sizeof(fat_dir_entry));
    mem::memcpy(new_entry->name, short_name, 11);
    new_entry->attr = dir_info.attributes;
    new_entry->first_cluster_high = (dir_info.first_cluster >> 16) & 0xFFFF;
    new_entry->first_cluster_low = dir_info.first_cluster & 0xFFFF;
    new_entry->file_size = 0;
    
    fat_write_dir_entries(parent_info.first_cluster, parent_entries, parent_count, parent_is_root);
    mem::heap::free(parent_entries);
    
    return true;
}

bool fat_move_dir(const char* old_path, const char* new_path) {
    if (!old_path || !new_path || old_path[0] != '/' || new_path[0] != '/') return false;
    
    fat_file_info dir_info;
    if (!fat_navigate_path(old_path, &dir_info) || !dir_info.is_directory) {
        return false;
    }
    
    if (dir_info.first_cluster == 0 && fat_type == FAT_TYPE::FAT16) {
        return false;
    }
    
    char new_filename[256];
    const char* last_slash = nullptr;
    for (const char* p = new_path; *p; p++) {
        if (*p == '/') last_slash = p;
    }
    if (!last_slash) return false;
    strcpy(new_filename, last_slash + 1);
    
    char new_parent_path[256];
    if (last_slash != new_path) {
        size_t len = last_slash - new_path;
        mem::memcpy(new_parent_path, new_path, len);
        new_parent_path[len] = '\0';
    } else {
        new_parent_path[0] = '/';
        new_parent_path[1] = '\0';
    }
    
    fat_file_info old_parent_info;
    char old_parent_path[256];
    const char* old_slash = nullptr;
    for (const char* p = old_path; *p; p++) {
        if (*p == '/') old_slash = p;
    }
    if (old_slash && old_slash != old_path) {
        size_t len = old_slash - old_path;
        mem::memcpy(old_parent_path, old_path, len);
        old_parent_path[len] = '\0';
    } else {
        old_parent_path[0] = '/';
        old_parent_path[1] = '\0';
    }
    if (!fat_navigate_path(old_parent_path, &old_parent_info)) return false;
    bool old_parent_is_root = (old_parent_info.first_cluster == 0 && fat_type == FAT_TYPE::FAT16);
    
    fat_file_info new_parent_info;
    if (!fat_navigate_path(new_parent_path, &new_parent_info) || !new_parent_info.is_directory) {
        return false;
    }
    bool new_parent_is_root = (new_parent_info.first_cluster == 0 && fat_type == FAT_TYPE::FAT16);
    
    fat_dir_entry* old_entries = nullptr;
    uint32_t old_count = 0;
    if (!fat_read_dir_entries(old_parent_info.first_cluster, &old_entries, &old_count, old_parent_is_root)) {
        return false;
    }
    
    uint32_t lfn_start = dir_info.dir_entry_offset;
    while (lfn_start > 0 && old_entries[lfn_start - 1].attr == ATTR_LFN) {
        lfn_start--;
    }
    for (uint32_t i = lfn_start; i <= dir_info.dir_entry_offset; i++) {
        old_entries[i].name[0] = 0xE5;
    }
    fat_write_dir_entries(old_parent_info.first_cluster, old_entries, old_count, old_parent_is_root);
    mem::heap::free(old_entries);
    
    uint8_t short_name[11];
    generate_short_name(new_filename, short_name);
    fat_dir_entry* lfn_entries = nullptr;
    uint32_t num_lfn = 0;
    bool use_lfn = strlen(new_filename) > 12;
    if (use_lfn) {
        if (!fat_create_lfn_entries(new_filename, short_name, &lfn_entries, &num_lfn)) {
            return false;
        }
    }
    
    uint32_t entry_idx = 0;
    uint32_t num_needed = (use_lfn ? num_lfn : 0) + 1;
    if (!fat_find_free_dir_entry(new_parent_info.first_cluster, &entry_idx, num_needed, new_parent_is_root)) {
        if (lfn_entries) mem::heap::free(lfn_entries);
        return false;
    }
    
    fat_dir_entry* new_entries = nullptr;
    uint32_t new_count = 0;
    if (!fat_read_dir_entries(new_parent_info.first_cluster, &new_entries, &new_count, new_parent_is_root)) {
        if (lfn_entries) mem::heap::free(lfn_entries);
        return false;
    }
    
    if (use_lfn) {
        for (uint32_t i = 0; i < num_lfn; i++) {
            new_entries[entry_idx + i] = lfn_entries[i];
        }
        mem::heap::free(lfn_entries);
    }
    
    fat_dir_entry* new_entry = &new_entries[entry_idx + (use_lfn ? num_lfn : 0)];
    mem::memset(new_entry, 0, sizeof(fat_dir_entry));
    mem::memcpy(new_entry->name, short_name, 11);
    new_entry->attr = dir_info.attributes;
    new_entry->first_cluster_high = (dir_info.first_cluster >> 16) & 0xFFFF;
    new_entry->first_cluster_low = dir_info.first_cluster & 0xFFFF;
    new_entry->file_size = 0;
    
    fat_write_dir_entries(new_parent_info.first_cluster, new_entries, new_count, new_parent_is_root);
    mem::heap::free(new_entries);
    
    uint16_t bytes_per_sector = (fat_type == FAT_TYPE::FAT32) ? g_fat32_bs->bpb.bytes_per_sector : g_fat16_bs->bpb.bytes_per_sector;
    uint8_t sectors_per_cluster = (fat_type == FAT_TYPE::FAT32) ? g_fat32_bs->bpb.sectors_per_cluster : g_fat16_bs->bpb.sectors_per_cluster;
    uint32_t cluster_size = bytes_per_sector * sectors_per_cluster;
    uint8_t* cluster_data = (uint8_t*)mem::heap::malloc(cluster_size);
    if (!cluster_data) return false;
    
    fat_read_cluster(dir_info.first_cluster, cluster_data);
    
    fat_dir_entry* dotdot = (fat_dir_entry*)(cluster_data + sizeof(fat_dir_entry));
    dotdot->first_cluster_high = (new_parent_info.first_cluster >> 16) & 0xFFFF;
    dotdot->first_cluster_low = new_parent_info.first_cluster & 0xFFFF;
    
    fat_write_cluster(dir_info.first_cluster, cluster_data);
    mem::heap::free(cluster_data);
    
    return true;
}

void fat_init() {
	FDPRINTF("Initialising FAT driver\n\r");
	uint8_t* bs_buffer = (uint8_t*)mem::heap::malloc(512);
	if (!bs_buffer) {
		FDPRINTF("Failed to allocate boot sector buffer\n\r");
		return;
	}
	drivers::blockio::diskgeneric::read(drivers::blockio::diskgeneric::get_boot_disk_sn(), drivers::blockio::diskgeneric::get_esp_part_sn(), 0, 1, bs_buffer, 512);
	fat_bpb* bpb = (fat_bpb*)bs_buffer;
	if (bpb->fat_size_16 == 0 && bpb->root_entry_count == 0) {
		fat_type = FAT_TYPE::FAT32;
		FDPRINTF("Detected FAT32\n\r");
		g_fat32_bs = (fat32_boot_sector*)mem::heap::malloc(sizeof(fat32_boot_sector));
		if (g_fat32_bs) {
			mem::memcpy(g_fat32_bs, bs_buffer, sizeof(fat32_boot_sector));
		}
		if (!g_fat32_bs || g_fat32_bs->ebpb.root_cluster == 0) {
			FDPRINTF("Error: FAT32 root_cluster is zero or allocation failed\n\r");
			mem::heap::free(bs_buffer);
			return;
		}
	} else {
		fat_type = FAT_TYPE::FAT16;
		FDPRINTF("Detected FAT16\n\r");
		g_fat16_bs = (fat16_boot_sector*)mem::heap::malloc(sizeof(fat16_boot_sector));
		if (g_fat16_bs) {
			mem::memcpy(g_fat16_bs, bs_buffer, sizeof(fat16_boot_sector));
		}
		if (!g_fat16_bs) {
			FDPRINTF("Error: FAT16 boot sector allocation failed\n\r");
			mem::heap::free(bs_buffer);
			return;
		}
	}
	mem::heap::free(bs_buffer);
	FDPRINTF("FAT init complete\n\r");
}
