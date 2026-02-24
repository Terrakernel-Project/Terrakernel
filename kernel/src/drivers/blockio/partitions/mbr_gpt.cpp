#include "../diskgeneric.hpp"
#include <panic.hpp>
#include <cstring>
#include <cstdio>
#include <mem/mem.hpp>
#include <config.hpp>

#ifdef CONFIG_PARTITONS_VERBOSE
#	define PDPRINTF(fmt, ...) printf("[ %s ] " fmt, __PRETTY_FUNCTION__, ##__VA_ARGS__)
#else
#	define PDPRINTF(fmt, ...)
#endif

struct mbr_partition_entry {
	uint8_t drive_attributes;
	uint8_t chs_part_start[3];
	uint8_t part_type;
	uint8_t chs_last_sect[3];
	uint32_t lba_part_start;
	uint32_t num_sects;
} __attribute__((packed));

struct mbr_struct {
	uint8_t mbr_bootstrap[440];
	uint32_t unique_disk_id;
	uint16_t reserved;
	mbr_partition_entry part_entries[4];
	uint16_t mbr_sig;
} __attribute__((packed));

/* ===================================== */

struct gpt_partition_entry {
	uint8_t part_type_guid[16];
	uint8_t uuid[16];
	uint64_t starting_lba;
	uint64_t ending_lba;
	uint64_t attrs;
	uint16_t part_name[36];  // 72 bytes = 36 UTF-16 chars
} __attribute__((packed));

#define GPT_SIG 0x5452415020494645ULL /* "EFI PART" */

struct gpt_header {
	uint64_t sig;
	uint32_t gpt_revision;
	uint32_t hdr_size;
	uint32_t crc32_checksum;
	uint32_t rsvd;
	uint64_t lba_this;
	uint64_t lba_alt_gpt_hdr;
	uint64_t first_usable_lba;
	uint64_t last_usable_lba;
	uint8_t disk_guid[16];
	uint64_t part_entry_lba;
	uint32_t num_part_entries;
	uint32_t part_entry_size;
	uint32_t part_array_crc32;
} __attribute__((packed));

// ESP (EFI System Partition) GUID: C12A7328-F81F-11D2-BA4B-00A0C93EC93B
static const uint8_t ESP_GUID[16] = {
	0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11,
	0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B
};

// Microsoft Basic Data GUID: EBD0A0A2-B9E5-4433-87C0-68B6B72699C7
static const uint8_t BASIC_DATA_GUID[16] = {
	0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44,
	0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7
};

// Linux filesystem GUID: 0FC63DAF-8483-4772-8E79-3D69D8477DE4
static const uint8_t LINUX_FS_GUID[16] = {
	0xAF, 0x3D, 0xC6, 0x0F, 0x83, 0x84, 0x72, 0x47,
	0x8E, 0x79, 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4
};

// Terrakernel filesystem GUID: 90990475-9B31-4304-955F-0BF8B80A69CB
static const uint8_t TERRAKERNEL_FS_GUID[16] = {
	0x75, 0x04, 0x99, 0x90, 0x31, 0x9B, 0x04, 0x43,
	0x5F, 0x95, 0xCB, 0x59, 0x0A, 0xB8, 0xF8, 0x0B
};

mbr_struct* mbr_sectors;
gpt_header* gpt_hdr;
bool is_gpt;

struct internal_part {
	uint64_t first_lba;
	uint64_t last_lba;
	char name[36];
	bool esp;
	bool formatted;
	bool valid;
};

internal_part parts[128];
int num_partitions = 0;

bool guid_compare(const uint8_t* guid1, const uint8_t* guid2) {
	for (int i = 0; i < 16; i++) {
		if (guid1[i] != guid2[i]) return false;
	}
	return true;
}

bool guid_is_zero(const uint8_t* guid) {
	for (int i = 0; i < 16; i++) {
		if (guid[i] != 0) return false;
	}
	return true;
}

void utf16_to_ascii(const uint16_t* utf16, char* ascii, size_t max_len) {
	size_t i;
	for (i = 0; i < max_len - 1 && utf16[i] != 0; i++) {
		ascii[i] = (char)(utf16[i] & 0xFF);
	}
	ascii[i] = '\0';
}

void prepare_part_table_gpt() {
	PDPRINTF("Parsing GPT partition table\n");
	PDPRINTF("GPT Header:\n");
	PDPRINTF("  Revision: 0x%08x\n", gpt_hdr->gpt_revision);
	PDPRINTF("  Header size: %u bytes\n", gpt_hdr->hdr_size);
	PDPRINTF("  First usable LBA: %lu\n", gpt_hdr->first_usable_lba);
	PDPRINTF("  Last usable LBA: %lu\n", gpt_hdr->last_usable_lba);
	PDPRINTF("  Partition entry LBA: %lu\n", gpt_hdr->part_entry_lba);
	PDPRINTF("  Number of partition entries: %u\n", gpt_hdr->num_part_entries);
	PDPRINTF("  Partition entry size: %u bytes\n", gpt_hdr->part_entry_size);
	
	uint32_t entries_to_read = gpt_hdr->num_part_entries;
	if (entries_to_read > 128) entries_to_read = 128;
	
	size_t entries_size = entries_to_read * gpt_hdr->part_entry_size;
	size_t sectors_needed = (entries_size + 511) / 512;
	
	uint8_t* entry_buffer = (uint8_t*)mem::heap::malloc(sectors_needed * 512);
	
	int64_t read_result = drivers::blockio::diskgeneric::raw_read(
		drivers::blockio::diskgeneric::get_boot_disk_sn(),
		gpt_hdr->part_entry_lba, 
		sectors_needed, 
		entry_buffer, 
		sectors_needed * 512
	);
	
	if (read_result != (int64_t)(sectors_needed * 512)) {
		PDPRINTF("Failed to read GPT partition entries\n");
		mem::heap::free(entry_buffer);
		return;
	}
	
	num_partitions = 0;
	
	for (uint32_t i = 0; i < entries_to_read; i++) {
		gpt_partition_entry* entry = (gpt_partition_entry*)(entry_buffer + (i * gpt_hdr->part_entry_size));
		
		if (guid_is_zero(entry->part_type_guid)) {
			continue;
		}
		
		parts[num_partitions].first_lba = entry->starting_lba;
		parts[num_partitions].last_lba = entry->ending_lba;
		parts[num_partitions].valid = true;
		
		utf16_to_ascii(entry->part_name, parts[num_partitions].name, 73);
		
		parts[num_partitions].esp = guid_compare(entry->part_type_guid, ESP_GUID);
		
		parts[num_partitions].formatted = 
			guid_compare(entry->part_type_guid, BASIC_DATA_GUID) ||
			guid_compare(entry->part_type_guid, LINUX_FS_GUID) ||
			parts[num_partitions].esp;
		
		PDPRINTF("Partition %d:\n", num_partitions);
		PDPRINTF("  Name: %s\n", parts[num_partitions].name);
		PDPRINTF("  LBA range: %lu - %lu\n", parts[num_partitions].first_lba, parts[num_partitions].last_lba);
		PDPRINTF("  Size: %lu sectors (%lu MB)\n", 
			parts[num_partitions].last_lba - parts[num_partitions].first_lba + 1,
			((parts[num_partitions].last_lba - parts[num_partitions].first_lba + 1) * 512) / (1024 * 1024));
		PDPRINTF("  ESP: %s\n", parts[num_partitions].esp ? "Yes" : "No");
		PDPRINTF("  Type GUID: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
			entry->part_type_guid[3], entry->part_type_guid[2], entry->part_type_guid[1], entry->part_type_guid[0],
			entry->part_type_guid[5], entry->part_type_guid[4],
			entry->part_type_guid[7], entry->part_type_guid[6],
			entry->part_type_guid[8], entry->part_type_guid[9],
			entry->part_type_guid[10], entry->part_type_guid[11], entry->part_type_guid[12],
			entry->part_type_guid[13], entry->part_type_guid[14], entry->part_type_guid[15]);
		
		num_partitions++;
	}
	
	mem::heap::free(entry_buffer);
	
	PDPRINTF("Found %d GPT partitions\n", num_partitions);
}

void prepare_part_table_mbr() {
	PDPRINTF("Parsing MBR partition table\n");
	
	if (mbr_sectors->mbr_sig != 0xAA55) {
		PDPRINTF("Invalid MBR signature: 0x%04x\n", mbr_sectors->mbr_sig);
		return;
	}
	
	num_partitions = 0;
	
	for (int i = 0; i < 4; i++) {
		mbr_partition_entry* entry = &mbr_sectors->part_entries[i];
		
		if (entry->part_type == 0x00) {
			continue;
		}
		
		parts[num_partitions].first_lba = entry->lba_part_start;
		parts[num_partitions].last_lba = entry->lba_part_start + entry->num_sects - 1;
		parts[num_partitions].valid = true;
		
		// MBR doesn't have names, so generate one
		snprintf(parts[num_partitions].name, 73, "Partition %d", i + 1);
		
		parts[num_partitions].esp = (entry->part_type == 0xEF);
		
		parts[num_partitions].formatted = (
			entry->part_type == 0x83 ||  // Linux
			entry->part_type == 0x07 ||  // NTFS/exFAT
			entry->part_type == 0x0B ||  // FAT32
			entry->part_type == 0x0C ||  // FAT32 LBA
			entry->part_type == 0xEF     // ESP
		);
		
		PDPRINTF("Partition %d:\n", num_partitions);
		PDPRINTF("  Type: 0x%02x\n", entry->part_type);
		PDPRINTF("  LBA range: %u - %lu\n", entry->lba_part_start, parts[num_partitions].last_lba);
		PDPRINTF("  Size: %u sectors (%lu MB)\n", 
			entry->num_sects,
			((uint64_t)entry->num_sects * 512) / (1024 * 1024));
		PDPRINTF("  Bootable: %s\n", (entry->drive_attributes & 0x80) ? "Yes" : "No");
		PDPRINTF("  ESP: %s\n", parts[num_partitions].esp ? "Yes" : "No");
		
		num_partitions++;
	}
	
	PDPRINTF("Found %d MBR partitions\n", num_partitions);
}

namespace drivers::blockio::diskgeneric::partitions {

void initialise() {
	PDPRINTF("Initializing partition table\n");
	
	uint8_t* sectors_1_to_3 = (uint8_t*)mem::heap::malloc(512*3);
	
	if (drivers::blockio::diskgeneric::raw_read(drivers::blockio::diskgeneric::get_boot_disk_sn(), 0, 3, sectors_1_to_3, 512 * 3) != 512 * 3) {
		panic("Failed to read disk sectors");
	}
	
	mbr_sectors = (mbr_struct*)sectors_1_to_3;
	gpt_hdr = (gpt_header*)(sectors_1_to_3 + 512);
	
	if (gpt_hdr->sig == GPT_SIG) {
		is_gpt = true;
		PDPRINTF("Detected GPT partition table\n");
		prepare_part_table_gpt();
	} else {
		is_gpt = false;
		PDPRINTF("Detected MBR partition table\n");
		prepare_part_table_mbr();
	}
}

internal_part* get_partition(int index) {
	if (index < 0 || index >= num_partitions) {
		return nullptr;
	}
	return &parts[index];
}

int get_partition_count() {
	return num_partitions;
}

internal_part* find_esp() {
	for (int i = 0; i < num_partitions; i++) {
		if (parts[i].esp && parts[i].valid) {
			return &parts[i];
		}
	}
	return nullptr;
}

uint64_t get_part_offset(int part_sn, uint64_t sect) {
	if (part_sn > num_partitions || part_sn < 0) return (uint64_t)-1;

	if (sect > (parts[part_sn].last_lba - parts[part_sn].first_lba)) return (uint64_t)-1;

	return (parts[part_sn].first_lba + sect);
}

int get_num_parts() {
	return num_partitions;
}

int get_esp_part_sn_intrnl() {
	for (int i = 0; i < num_partitions; i++) {
		if (parts[i].esp) return i;
	}
	return -1;
}

}
