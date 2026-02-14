#include "fat32.hpp"
#include <drivers/blockio/diskgeneric.hpp>
#include <drivers/fs/fsgeneric.hpp>
#include <mem/mem.hpp>
#include <string.h>
#include <cstdio>

static fat32_state g_fat32;

void fat32_init() {
    FDPRINTF("Initializing FAT32 driver\n");
    mem::memset(&g_fat32, 0, sizeof(fat32_state));
    g_fat32.part_sn = 1;
    g_fat32.fat_cache = nullptr;
    g_fat32.fat_cache_sector = 0xFFFFFFFF;
    g_fat32.fat_cache_valid = false;
    
    g_fat32.fat_cache = (uint8_t*)mem::heap::malloc(512);
    FDPRINTF("Allocated FAT cache at %p\n", g_fat32.fat_cache);
    
    if (!fat32_read_boot_sector()) {
        FDPRINTF("Failed to read boot sector\n");
        return;
    }

	if (cluster_count < 4085) {
	    g_fat.type = FATType::FAT12;
	} else if (cluster_count < 65525) {
	    g_fat.type = FATType::FAT16;
	} else {
	    g_fat.type = FATType::FAT32;
	}
    
    FDPRINTF("FAT32 init complete\n");
}

bool fat32_read_boot_sector() {
    FDPRINTF("Reading boot sector from partition %d\n", g_fat32.part_sn);
    uint8_t* boot_sector = (uint8_t*)mem::heap::malloc(512);
    
    int64_t result = drivers::blockio::diskgeneric::read(
        g_fat32.part_sn, 0, 1, boot_sector, 512
    );
    
    FDPRINTF("Boot sector read result: %lld bytes\n", result);
    
    if (result < 0) {
        FDPRINTF("Failed to read boot sector (error %lld)\n", result);
        mem::heap::free(boot_sector);
        return false;
    }
    
    FDPRINTF("First 16 bytes: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
             boot_sector[0], boot_sector[1], boot_sector[2], boot_sector[3],
             boot_sector[4], boot_sector[5], boot_sector[6], boot_sector[7],
             boot_sector[8], boot_sector[9], boot_sector[10], boot_sector[11],
             boot_sector[12], boot_sector[13], boot_sector[14], boot_sector[15]);
    
    mem::memcpy(&g_fat32.bpb, boot_sector, sizeof(fat32_bpb));
    
    FDPRINTF("OEM Name: %.8s\n", g_fat32.bpb.oem_name);
    FDPRINTF("FS Type field (offset 54 for FAT12/16): %.8s\n", &boot_sector[54]);
    FDPRINTF("FS Type field (offset 82 for FAT32): %.8s\n", g_fat32.bpb.fs_type);
    FDPRINTF("Boot signature offset 66 (FAT32): 0x%02x\n", boot_sector[66]);
    FDPRINTF("Boot signature offset 38 (FAT12/16): 0x%02x\n", boot_sector[38]);
    FDPRINTF("Bytes 510-511: 0x%02x 0x%02x (expecting 0x55 0xAA)\n", boot_sector[510], boot_sector[511]);
    
    if (boot_sector[510] != 0x55 || boot_sector[511] != 0xAA) {
        FDPRINTF("Invalid boot sector signature at end\n", "");
        mem::heap::free(boot_sector);
        return false;
    }
    
    uint32_t total_sectors = g_fat32.bpb.total_sectors_16;
    if (total_sectors == 0) {
        total_sectors = g_fat32.bpb.total_sectors_32;
    }
    
    uint32_t fat_size = g_fat32.bpb.fat_size_16;
    if (fat_size == 0) {
        fat_size = g_fat32.bpb.fat_size_32;
    }
    
    uint32_t root_dir_sectors = ((g_fat32.bpb.root_entry_count * 32) + (g_fat32.bpb.bytes_per_sector - 1)) / g_fat32.bpb.bytes_per_sector;
    uint32_t data_sectors = total_sectors - (g_fat32.bpb.reserved_sectors + (g_fat32.bpb.num_fats * fat_size) + root_dir_sectors);
    uint32_t cluster_count = data_sectors / g_fat32.bpb.sectors_per_cluster;
    
    FDPRINTF("Cluster count: %u\n", cluster_count);
    FDPRINTF("Root entry count: %u (FAT12/16 only)\n", g_fat32.bpb.root_entry_count);
    
    bool is_fat32 = false;
    if (cluster_count < 4085) {
        FDPRINTF("Detected FAT12 (cluster_count=%u < 4085)\n", cluster_count);
    } else if (cluster_count < 65525) {
        FDPRINTF("Detected FAT16 (4085 <= cluster_count=%u < 65525)\n", cluster_count);
    } else {
        FDPRINTF("Detected FAT32 (cluster_count=%u >= 65525)\n", cluster_count);
        is_fat32 = true;
    }
    
    if (!is_fat32) {
        if (boot_sector[38] != 0x29 && boot_sector[38] != 0x28) {
            FDPRINTF("Invalid FAT12/16 boot signature: 0x%02x at offset 38\n", boot_sector[38]);
            mem::heap::free(boot_sector);
            return false;
        }
        
        FDPRINTF("FAT12/16 detected - converting to FAT32-compatible structure\n", "");
        g_fat32.bpb.boot_signature = boot_sector[38];
        g_fat32.bpb.fat_size_32 = g_fat32.bpb.fat_size_16;
        g_fat32.bpb.root_cluster = 0;
        
        mem::memcpy(g_fat32.bpb.volume_label, &boot_sector[43], 11);
        mem::memcpy(g_fat32.bpb.fs_type, &boot_sector[54], 8);
    } else {
        if (g_fat32.bpb.boot_signature != 0x29 && g_fat32.bpb.boot_signature != 0x28) {
            FDPRINTF("Invalid FAT32 boot signature: 0x%02x at offset 66\n", g_fat32.bpb.boot_signature);
            
            printf("\n[FAT32] Boot sector hex dump:\n");
            for (int y = 0; y < 32; y++) {
                printf("%04x | ", y*16);
                for (int x = 0; x < 16; x++) {
                    printf("%02x ", boot_sector[y * 16 + x]);
                }
                printf("| ");
                for (int x = 0; x < 16; x++) {
                    char c = boot_sector[y * 16 + x];
                    printf("%c", (c >= 32 && c <= 126) ? c : '.');
                }
                printf("\n");
            }
            printf("\n");
            
            mem::heap::free(boot_sector);
            return false;
        }
    }
    
    mem::heap::free(boot_sector);
    
    g_fat32.fat_begin_lba = g_fat32.bpb.reserved_sectors;
    g_fat32.fat_size_sectors = (g_fat32.bpb.fat_size_32 == 0) ? g_fat32.bpb.fat_size_16 : g_fat32.bpb.fat_size_32;
    
    if (g_fat32.bpb.root_cluster == 0) {
        g_fat32.cluster_begin_lba = g_fat32.fat_begin_lba + 
                                    (g_fat32.bpb.num_fats * g_fat32.fat_size_sectors) +
                                    root_dir_sectors;
        g_fat32.root_dir_cluster = 2;
    } else {
        g_fat32.cluster_begin_lba = g_fat32.fat_begin_lba + 
                                    (g_fat32.bpb.num_fats * g_fat32.fat_size_sectors);
        g_fat32.root_dir_cluster = g_fat32.bpb.root_cluster;
    }
    
    g_fat32.sectors_per_cluster = g_fat32.bpb.sectors_per_cluster;
    g_fat32.bytes_per_cluster = g_fat32.sectors_per_cluster * g_fat32.bpb.bytes_per_sector;
    
    FDPRINTF("Bytes per sector: %u\n", g_fat32.bpb.bytes_per_sector);
    FDPRINTF("Sectors per cluster: %u\n", g_fat32.sectors_per_cluster);
    FDPRINTF("Root cluster: %u\n", g_fat32.root_dir_cluster);
    FDPRINTF("FAT begin LBA: %u\n", g_fat32.fat_begin_lba);
    FDPRINTF("Cluster begin LBA: %u\n", g_fat32.cluster_begin_lba);
    FDPRINTF("Volume label: %.11s\n", g_fat32.bpb.volume_label);
    FDPRINTF("FS type: %.8s\n", g_fat32.bpb.fs_type);
    
    return true;
}

uint32_t fat32_cluster_to_lba(uint32_t cluster) {
    if (cluster < 2) {
        return 0;
    }
    return g_fat32.cluster_begin_lba + ((cluster - 2) * g_fat32.sectors_per_cluster);
}

uint32_t fat32_get_next_cluster(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = g_fat32.fat_begin_lba + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;
    
    if (!g_fat32.fat_cache_valid || g_fat32.fat_cache_sector != fat_sector) {
        int64_t result = drivers::blockio::diskgeneric::read(
            g_fat32.part_sn, fat_sector, 1, g_fat32.fat_cache, 512
        );
        
        if (result < 0) {
            return FAT32_CLUSTER_EOC;
        }
        
        g_fat32.fat_cache_sector = fat_sector;
        g_fat32.fat_cache_valid = true;
    }
    
    uint32_t next_cluster = *((uint32_t*)(g_fat32.fat_cache + entry_offset));
    next_cluster &= FAT32_CLUSTER_MASK;
    
    return next_cluster;
}

bool fat32_write_fat_entry(uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = g_fat32.fat_begin_lba + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;
    
    uint8_t* sector_buf = (uint8_t*)mem::heap::malloc(512);
    
    int64_t result = drivers::blockio::diskgeneric::read(
        g_fat32.part_sn, fat_sector, 1, sector_buf, 512
    );
    
    if (result < 0) {
        mem::heap::free(sector_buf);
        return false;
    }
    
    uint32_t* entry = (uint32_t*)(sector_buf + entry_offset);
    *entry = (*entry & 0xF0000000) | (value & FAT32_CLUSTER_MASK);
    
    for (uint8_t i = 0; i < g_fat32.bpb.num_fats; i++) {
        uint32_t current_fat_sector = fat_sector + (i * g_fat32.fat_size_sectors);
        result = drivers::blockio::diskgeneric::write(
            g_fat32.part_sn, current_fat_sector, 1, sector_buf, 512
        );
        if (result < 0) {
            mem::heap::free(sector_buf);
            return false;
        }
    }
    
    mem::heap::free(sector_buf);
    g_fat32.fat_cache_valid = false;
    return true;
}

uint32_t fat32_allocate_cluster() {
    uint32_t total_clusters = g_fat32.fat_size_sectors * 512 / 4;
    
    for (uint32_t cluster = 2; cluster < total_clusters; cluster++) {
        uint32_t entry = fat32_get_next_cluster(cluster);
        if (entry == FAT32_CLUSTER_FREE) {
            if (fat32_write_fat_entry(cluster, FAT32_CLUSTER_EOC)) {
                return cluster;
            }
        }
    }
    
    return 0;
}

bool fat32_free_cluster_chain(uint32_t first_cluster) {
    uint32_t current = first_cluster;
    
    while (current >= 2 && current < FAT32_CLUSTER_EOC) {
        uint32_t next = fat32_get_next_cluster(current);
        if (!fat32_write_fat_entry(current, FAT32_CLUSTER_FREE)) {
            return false;
        }
        current = next;
    }
    
    return true;
}

bool fat32_extend_file(uint32_t* last_cluster, uint32_t clusters_needed) {
    uint32_t current = *last_cluster;
    
    for (uint32_t i = 0; i < clusters_needed; i++) {
        uint32_t new_cluster = fat32_allocate_cluster();
        if (new_cluster == 0) {
            return false;
        }
        
        if (current != 0) {
            if (!fat32_write_fat_entry(current, new_cluster)) {
                fat32_free_cluster_chain(new_cluster);
                return false;
            }
        }
        
        current = new_cluster;
    }
    
    *last_cluster = current;
    return true;
}

bool fat32_read_cluster(uint32_t cluster, uint8_t* buffer) {
    uint32_t lba = fat32_cluster_to_lba(cluster);
    if (lba == 0) {
        FDPRINTF("Invalid cluster %u (LBA = 0)\n", cluster);
        return false;
    }
    
    FDPRINTF("Reading cluster %u at LBA %u (%u sectors)\n", cluster, lba, g_fat32.sectors_per_cluster);
    
    int64_t result = drivers::blockio::diskgeneric::read(
        g_fat32.part_sn, lba, g_fat32.sectors_per_cluster, 
        buffer, g_fat32.bytes_per_cluster
    );
    
    FDPRINTF("Read cluster result: %lld bytes\n", result);
    
    return result >= 0;
}

bool fat32_write_cluster(uint32_t cluster, const uint8_t* buffer) {
    uint32_t lba = fat32_cluster_to_lba(cluster);
    if (lba == 0) {
        FDPRINTF("Invalid cluster %u (LBA = 0)\n", cluster);
        return false;
    }
    
    FDPRINTF("Writing cluster %u at LBA %u\n", cluster, lba);
    
    int64_t result = drivers::blockio::diskgeneric::write(
        g_fat32.part_sn, lba, g_fat32.sectors_per_cluster, 
        buffer, g_fat32.bytes_per_cluster
    );
    
    FDPRINTF("Write cluster result: %lld bytes\n", result);
    
    return result >= 0;
}

uint8_t fat32_lfn_checksum(const uint8_t* short_name) {
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++) {
        sum = ((sum & 1) << 7) + (sum >> 1) + short_name[i];
    }
    return sum;
}

void fat32_filename_to_83(const char* filename, uint8_t* name_83) {
    mem::memset(name_83, ' ', 11);
    
    const char* ext = nullptr;
    for (int i = strlen(filename) - 1; i >= 0; i--) {
        if (filename[i] == '.') {
            ext = &filename[i + 1];
            break;
        }
    }
    
    int name_len = ext ? (ext - filename - 1) : strlen(filename);
    if (name_len > 8) name_len = 8;
    
    for (int i = 0; i < name_len; i++) {
        char c = filename[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        name_83[i] = c;
    }
    
    if (ext) {
        int ext_len = strlen(ext);
        if (ext_len > 3) ext_len = 3;
        for (int i = 0; i < ext_len; i++) {
            char c = ext[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            name_83[8 + i] = c;
        }
    }
}

void fat32_83_to_filename(const uint8_t* name_83, char* filename) {
    int pos = 0;
    
    int name_end = 8;
    while (name_end > 0 && name_83[name_end - 1] == ' ') {
        name_end--;
    }
    
    for (int i = 0; i < name_end; i++) {
        filename[pos++] = name_83[i];
    }
    
    int ext_end = 11;
    while (ext_end > 8 && name_83[ext_end - 1] == ' ') {
        ext_end--;
    }
    
    if (ext_end > 8) {
        filename[pos++] = '.';
        for (int i = 8; i < ext_end; i++) {
            filename[pos++] = name_83[i];
        }
    }
    
    filename[pos] = '\0';
}

bool fat32_match_filename(const uint8_t* name_83, const char* filename) {
    uint8_t test_name[11];
    fat32_filename_to_83(filename, test_name);
    return mem::memcmp(name_83, test_name, 11) == 0;
}

bool fat32_parse_lfn(const fat32_lfn_entry* lfn_entries, size_t count, 
                     char* output, size_t output_size) {
    size_t pos = 0;
    
    for (int entry_idx = count - 1; entry_idx >= 0; entry_idx--) {
        const fat32_lfn_entry* lfn = &lfn_entries[entry_idx];
        
        for (int i = 0; i < 5; i++) {
            uint16_t c = lfn->name1[i];
            if (c == 0x0000 || c == 0xFFFF) goto done;
            if (pos < output_size - 1) {
                output[pos++] = (c < 0x80) ? (char)c : '?';
            }
        }
        
        for (int i = 0; i < 6; i++) {
            uint16_t c = lfn->name2[i];
            if (c == 0x0000 || c == 0xFFFF) goto done;
            if (pos < output_size - 1) {
                output[pos++] = (c < 0x80) ? (char)c : '?';
            }
        }
        
        for (int i = 0; i < 2; i++) {
            uint16_t c = lfn->name3[i];
            if (c == 0x0000 || c == 0xFFFF) goto done;
            if (pos < output_size - 1) {
                output[pos++] = (c < 0x80) ? (char)c : '?';
            }
        }
    }
    
done:
    output[pos] = '\0';
    return true;
}

bool fat32_read_directory_entries(uint32_t cluster, fat32_dir_entry* entries, 
                                   size_t* count, size_t max_entries) {
    *count = 0;
    uint8_t* cluster_buffer = (uint8_t*)mem::heap::malloc(g_fat32.bytes_per_cluster);
    
    uint32_t current_cluster = cluster;
    
    while (current_cluster < FAT32_CLUSTER_EOC) {
        if (!fat32_read_cluster(current_cluster, cluster_buffer)) {
            mem::heap::free(cluster_buffer);
            return false;
        }
        
        fat32_dir_entry* dir_entries = (fat32_dir_entry*)cluster_buffer;
        size_t entries_per_cluster = g_fat32.bytes_per_cluster / sizeof(fat32_dir_entry);
        
        for (size_t i = 0; i < entries_per_cluster; i++) {
            if (dir_entries[i].name[0] == 0x00) {
                mem::heap::free(cluster_buffer);
                return true;
            }
            
            if (dir_entries[i].name[0] == 0xE5) {
                continue;
            }
            
			if (dir_entries[i].attr == FAT32_ATTR_LONG_NAME ||
			    dir_entries[i].attr == FAT32_ATTR_VOLUME_ID) {
			    continue;
			}
            
            if (*count < max_entries) {
                mem::memcpy(&entries[*count], &dir_entries[i], sizeof(fat32_dir_entry));
                (*count)++;
            }
        }
        
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    
    mem::heap::free(cluster_buffer);
    return true;
}

bool fat32_find_file_in_directory(uint32_t dir_cluster, const char* filename, 
                                   fat32_dir_entry* out_entry, uint32_t* out_cluster) {
    FDPRINTF("Searching for '%s' in directory cluster %u\n", filename, dir_cluster);
    uint8_t* cluster_buffer = (uint8_t*)mem::heap::malloc(g_fat32.bytes_per_cluster);
    uint32_t current_cluster = dir_cluster;
    
    fat32_lfn_entry lfn_entries[20];
    size_t lfn_count = 0;
    uint8_t expected_checksum = 0;
    
    while (current_cluster < FAT32_CLUSTER_EOC) {
        if (!fat32_read_cluster(current_cluster, cluster_buffer)) {
            FDPRINTF("Failed to read directory cluster %u\n", current_cluster);
            mem::heap::free(cluster_buffer);
            return false;
        }
        
        fat32_dir_entry* dir_entries = (fat32_dir_entry*)cluster_buffer;
        size_t entries_per_cluster = g_fat32.bytes_per_cluster / sizeof(fat32_dir_entry);
        
        for (size_t i = 0; i < entries_per_cluster; i++) {
            if (dir_entries[i].name[0] == 0x00) {
                FDPRINTF("End of directory reached\n");
                mem::heap::free(cluster_buffer);
                return false;
            }
            
            if (dir_entries[i].name[0] == 0xE5) {
                lfn_count = 0;
                continue;
            }
            
            if (dir_entries[i].attr == FAT32_ATTR_LONG_NAME) {
                fat32_lfn_entry* lfn = (fat32_lfn_entry*)&dir_entries[i];
                if (lfn_count < 20) {
                    mem::memcpy(&lfn_entries[lfn_count], lfn, sizeof(fat32_lfn_entry));
                    lfn_count++;
                    if (lfn->sequence & 0x40) {
                        expected_checksum = lfn->checksum;
                    }
                }
                continue;
            }
            
            bool match = false;
            
            if (lfn_count > 0) {
                uint8_t checksum = fat32_lfn_checksum(dir_entries[i].name);
                if (checksum == expected_checksum) {
                    char lfn_name[256];
                    fat32_parse_lfn(lfn_entries, lfn_count, lfn_name, sizeof(lfn_name));
                    
                    if (strcasecmp(lfn_name, filename) == 0) {
                        FDPRINTF("Found file via LFN: %s\n", lfn_name);
                        match = true;
                    }
                }
            }
            
            if (!match && fat32_match_filename(dir_entries[i].name, filename)) {
                FDPRINTF("Found file via 8.3 name\n");
                match = true;
            }
            
            if (match) {
                mem::memcpy(out_entry, &dir_entries[i], sizeof(fat32_dir_entry));
                *out_cluster = ((uint32_t)dir_entries[i].first_cluster_hi << 16) | 
                               dir_entries[i].first_cluster_lo;
                FDPRINTF("File found: cluster=%u, size=%u\n", *out_cluster, out_entry->file_size);
                mem::heap::free(cluster_buffer);
                return true;
            }
            
            lfn_count = 0;
        }
        
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    
    FDPRINTF("File '%s' not found\n", filename);
    mem::heap::free(cluster_buffer);
    return false;
}

bool fat32_find_free_entry(uint32_t dir_cluster, uint32_t* entry_cluster, 
                           uint32_t* entry_offset) {
    uint8_t* cluster_buffer = (uint8_t*)mem::heap::malloc(g_fat32.bytes_per_cluster);
    uint32_t current_cluster = dir_cluster;
    
    while (current_cluster < FAT32_CLUSTER_EOC) {
        if (!fat32_read_cluster(current_cluster, cluster_buffer)) {
            mem::heap::free(cluster_buffer);
            return false;
        }
        
        fat32_dir_entry* dir_entries = (fat32_dir_entry*)cluster_buffer;
        size_t entries_per_cluster = g_fat32.bytes_per_cluster / sizeof(fat32_dir_entry);
        
        for (size_t i = 0; i < entries_per_cluster; i++) {
            if (dir_entries[i].name[0] == 0x00 || dir_entries[i].name[0] == 0xE5) {
                *entry_cluster = current_cluster;
                *entry_offset = i * sizeof(fat32_dir_entry);
                mem::heap::free(cluster_buffer);
                return true;
            }
        }
        
        uint32_t next = fat32_get_next_cluster(current_cluster);
        if (next >= FAT32_CLUSTER_EOC) {
            uint32_t new_cluster = fat32_allocate_cluster();
            if (new_cluster == 0) {
                mem::heap::free(cluster_buffer);
                return false;
            }
            
            if (!fat32_write_fat_entry(current_cluster, new_cluster)) {
                fat32_free_cluster_chain(new_cluster);
                mem::heap::free(cluster_buffer);
                return false;
            }
            
            mem::memset(cluster_buffer, 0, g_fat32.bytes_per_cluster);
            if (!fat32_write_cluster(new_cluster, cluster_buffer)) {
                mem::heap::free(cluster_buffer);
                return false;
            }
            
            *entry_cluster = new_cluster;
            *entry_offset = 0;
            mem::heap::free(cluster_buffer);
            return true;
        }
        
        current_cluster = next;
    }
    
    mem::heap::free(cluster_buffer);
    return false;
}

bool fat32_write_dir_entry(uint32_t cluster, uint32_t offset, 
                           const fat32_dir_entry* entry) {
    uint8_t* cluster_buffer = (uint8_t*)mem::heap::malloc(g_fat32.bytes_per_cluster);
    
    if (!fat32_read_cluster(cluster, cluster_buffer)) {
        mem::heap::free(cluster_buffer);
        return false;
    }
    
    mem::memcpy(cluster_buffer + offset, entry, sizeof(fat32_dir_entry));
    
    bool result = fat32_write_cluster(cluster, cluster_buffer);
    mem::heap::free(cluster_buffer);
    
    return result;
}

bool fat32_mark_entry_deleted(uint32_t dir_cluster, const char* filename) {
    uint8_t* cluster_buffer = (uint8_t*)mem::heap::malloc(g_fat32.bytes_per_cluster);
    uint32_t current_cluster = dir_cluster;
    
    while (current_cluster < FAT32_CLUSTER_EOC) {
        if (!fat32_read_cluster(current_cluster, cluster_buffer)) {
            mem::heap::free(cluster_buffer);
            return false;
        }
        
        fat32_dir_entry* dir_entries = (fat32_dir_entry*)cluster_buffer;
        size_t entries_per_cluster = g_fat32.bytes_per_cluster / sizeof(fat32_dir_entry);
        
        bool found = false;
        for (size_t i = 0; i < entries_per_cluster; i++) {
            if (dir_entries[i].name[0] == 0x00) {
                break;
            }
            
            if (dir_entries[i].name[0] == 0xE5) {
                continue;
            }
            
            if (dir_entries[i].attr != FAT32_ATTR_LONG_NAME &&
                fat32_match_filename(dir_entries[i].name, filename)) {
                dir_entries[i].name[0] = 0xE5;
                found = true;
                break;
            }
        }
        
        if (found) {
            bool result = fat32_write_cluster(current_cluster, cluster_buffer);
            mem::heap::free(cluster_buffer);
            return result;
        }
        
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    
    mem::heap::free(cluster_buffer);
    return false;
}

bool fat32_update_dir_entry(uint32_t dir_cluster, const char* filename,
                            const fat32_dir_entry* new_entry) {
    uint8_t* cluster_buffer = (uint8_t*)mem::heap::malloc(g_fat32.bytes_per_cluster);
    uint32_t current_cluster = dir_cluster;
    
    while (current_cluster < FAT32_CLUSTER_EOC) {
        if (!fat32_read_cluster(current_cluster, cluster_buffer)) {
            mem::heap::free(cluster_buffer);
            return false;
        }
        
        fat32_dir_entry* dir_entries = (fat32_dir_entry*)cluster_buffer;
        size_t entries_per_cluster = g_fat32.bytes_per_cluster / sizeof(fat32_dir_entry);
        
        bool found = false;
        for (size_t i = 0; i < entries_per_cluster; i++) {
            if (dir_entries[i].name[0] == 0x00) {
                break;
            }
            
            if (dir_entries[i].name[0] == 0xE5) {
                continue;
            }
            
            if (dir_entries[i].attr != FAT32_ATTR_LONG_NAME &&
                fat32_match_filename(dir_entries[i].name, filename)) {
                mem::memcpy(&dir_entries[i], new_entry, sizeof(fat32_dir_entry));
                found = true;
                break;
            }
        }
        
        if (found) {
            bool result = fat32_write_cluster(current_cluster, cluster_buffer);
            mem::heap::free(cluster_buffer);
            return result;
        }
        
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    
    mem::heap::free(cluster_buffer);
    return false;
}

bool fat32_create_file(uint32_t dir_cluster, const char* filename,
                       uint32_t* out_cluster) {
    fat32_dir_entry existing_entry;
    uint32_t existing_cluster;
    
    if (fat32_find_file_in_directory(dir_cluster, filename, &existing_entry, &existing_cluster)) {
        return false;
    }
    
    uint32_t new_cluster = fat32_allocate_cluster();
    if (new_cluster == 0) {
        return false;
    }
    
    uint32_t entry_cluster, entry_offset;
    if (!fat32_find_free_entry(dir_cluster, &entry_cluster, &entry_offset)) {
        fat32_free_cluster_chain(new_cluster);
        return false;
    }
    
    fat32_dir_entry new_entry;
    mem::memset(&new_entry, 0, sizeof(fat32_dir_entry));
    fat32_filename_to_83(filename, new_entry.name);
    new_entry.attr = FAT32_ATTR_ARCHIVE;
    new_entry.first_cluster_hi = (new_cluster >> 16) & 0xFFFF;
    new_entry.first_cluster_lo = new_cluster & 0xFFFF;
    new_entry.file_size = 0;
    
    if (!fat32_write_dir_entry(entry_cluster, entry_offset, &new_entry)) {
        fat32_free_cluster_chain(new_cluster);
        return false;
    }
    
    *out_cluster = new_cluster;
    return true;
}

bool fat32_create_directory(uint32_t parent_cluster, const char* dirname,
                            uint32_t* out_cluster) {
    fat32_dir_entry existing_entry;
    uint32_t existing_cluster;
    
    if (fat32_find_file_in_directory(parent_cluster, dirname, &existing_entry, &existing_cluster)) {
        return false;
    }
    
    uint32_t new_cluster = fat32_allocate_cluster();
    if (new_cluster == 0) {
        return false;
    }
    
    uint8_t* cluster_buffer = (uint8_t*)mem::heap::malloc(g_fat32.bytes_per_cluster);
    mem::memset(cluster_buffer, 0, g_fat32.bytes_per_cluster);
    
    fat32_dir_entry* entries = (fat32_dir_entry*)cluster_buffer;
    
    mem::memset(&entries[0], 0, sizeof(fat32_dir_entry));
    mem::memset(entries[0].name, ' ', 11);
    entries[0].name[0] = '.';
    entries[0].attr = FAT32_ATTR_DIRECTORY;
    entries[0].first_cluster_hi = (new_cluster >> 16) & 0xFFFF;
    entries[0].first_cluster_lo = new_cluster & 0xFFFF;
    
    mem::memset(&entries[1], 0, sizeof(fat32_dir_entry));
    mem::memset(entries[1].name, ' ', 11);
    entries[1].name[0] = '.';
    entries[1].name[1] = '.';
    entries[1].attr = FAT32_ATTR_DIRECTORY;
    entries[1].first_cluster_hi = (parent_cluster >> 16) & 0xFFFF;
    entries[1].first_cluster_lo = parent_cluster & 0xFFFF;
    
    if (!fat32_write_cluster(new_cluster, cluster_buffer)) {
        mem::heap::free(cluster_buffer);
        fat32_free_cluster_chain(new_cluster);
        return false;
    }
    
    mem::heap::free(cluster_buffer);
    
    uint32_t entry_cluster, entry_offset;
    if (!fat32_find_free_entry(parent_cluster, &entry_cluster, &entry_offset)) {
        fat32_free_cluster_chain(new_cluster);
        return false;
    }
    
    fat32_dir_entry new_entry;
    mem::memset(&new_entry, 0, sizeof(fat32_dir_entry));
    fat32_filename_to_83(dirname, new_entry.name);
    new_entry.attr = FAT32_ATTR_DIRECTORY;
    new_entry.first_cluster_hi = (new_cluster >> 16) & 0xFFFF;
    new_entry.first_cluster_lo = new_cluster & 0xFFFF;
    new_entry.file_size = 0;
    
    if (!fat32_write_dir_entry(entry_cluster, entry_offset, &new_entry)) {
        fat32_free_cluster_chain(new_cluster);
        return false;
    }
    
    *out_cluster = new_cluster;
    return true;
}

bool fat32_parse_path(const char* path, char* components[], size_t* count) {
    *count = 0;
    
    if (path[0] == '/') {
        path++;
    }
    
    char* path_copy = (char*)mem::heap::malloc(strlen(path) + 1);
    strcpy(path_copy, path);
    
    char* token = strtok(path_copy, "/");
    while (token != nullptr && *count < 32) {
        components[*count] = (char*)mem::heap::malloc(strlen(token) + 1);
        strcpy(components[*count], token);
        (*count)++;
        token = strtok(nullptr, "/");
    }
    
    mem::heap::free(path_copy);
    return true;
}

bool fat32_navigate_path(const char* path, uint32_t* out_cluster, bool* is_directory) {
    FDPRINTF("Navigating path: %s\n", path);
    char* components[32];
    size_t count;
    
    if (!fat32_parse_path(path, components, &count)) {
        FDPRINTF("Failed to parse path\n");
        return false;
    }
    
    FDPRINTF("Path has %zu components\n", count);
    
    uint32_t current_cluster = g_fat32.root_dir_cluster;
    *is_directory = true;
    
    for (size_t i = 0; i < count; i++) {
        FDPRINTF("Looking for component: %s\n", components[i]);
        fat32_dir_entry entry;
        uint32_t next_cluster;
        
        if (!fat32_find_file_in_directory(current_cluster, components[i], 
                                          &entry, &next_cluster)) {
            FDPRINTF("Component not found: %s\n", components[i]);
            for (size_t j = 0; j < count; j++) {
                mem::heap::free(components[j]);
            }
            return false;
        }
        
        current_cluster = next_cluster;
        *is_directory = (entry.attr & FAT32_ATTR_DIRECTORY) != 0;
        
        FDPRINTF("Found component at cluster %u, is_dir=%d\n", current_cluster, *is_directory);
        
        if (i < count - 1 && !*is_directory) {
            FDPRINTF("Path component is not a directory\n");
            for (size_t j = 0; j < count; j++) {
                mem::heap::free(components[j]);
            }
            return false;
        }
    }
    
    *out_cluster = current_cluster;
    FDPRINTF("Navigation complete: cluster=%u\n", current_cluster);
    
    for (size_t i = 0; i < count; i++) {
        mem::heap::free(components[i]);
    }
    
    return true;
}

bool fat32_read_file(uint32_t first_cluster, uint32_t file_size, 
                     uint8_t* buffer, size_t buffer_size) {
    FDPRINTF("Reading file: cluster=%u, size=%u, buffer_size=%zu\n", first_cluster, file_size, buffer_size);
    
    if (buffer_size < file_size) {
        FDPRINTF("Buffer too small: %zu < %u\n", buffer_size, file_size);
        return false;
    }
    
    uint32_t bytes_read = 0;
    uint32_t current_cluster = first_cluster;
    uint8_t* cluster_buffer = (uint8_t*)mem::heap::malloc(g_fat32.bytes_per_cluster);
    
    while (current_cluster < FAT32_CLUSTER_EOC && bytes_read < file_size) {
        FDPRINTF("Reading file cluster %u (bytes_read=%u/%u)\n", current_cluster, bytes_read, file_size);
        
        if (!fat32_read_cluster(current_cluster, cluster_buffer)) {
            FDPRINTF("Failed to read cluster %u\n", current_cluster);
            mem::heap::free(cluster_buffer);
            return false;
        }
        
        uint32_t bytes_to_copy = g_fat32.bytes_per_cluster;
        if (bytes_read + bytes_to_copy > file_size) {
            bytes_to_copy = file_size - bytes_read;
        }
        
        mem::memcpy(buffer + bytes_read, cluster_buffer, bytes_to_copy);
        bytes_read += bytes_to_copy;
        
        current_cluster = fat32_get_next_cluster(current_cluster);
        FDPRINTF("Next cluster: %u\n", current_cluster);
    }
    
    mem::heap::free(cluster_buffer);
    FDPRINTF("File read complete: %u bytes\n", bytes_read);
    return bytes_read == file_size;
}

bool fat32_write_file(uint32_t first_cluster, const uint8_t* data, uint32_t data_size) {
    uint32_t bytes_written = 0;
    uint32_t current_cluster = first_cluster;
    uint8_t* cluster_buffer = (uint8_t*)mem::heap::malloc(g_fat32.bytes_per_cluster);
    
    while (current_cluster < FAT32_CLUSTER_EOC && bytes_written < data_size) {
        uint32_t bytes_to_copy = g_fat32.bytes_per_cluster;
        if (bytes_written + bytes_to_copy > data_size) {
            bytes_to_copy = data_size - bytes_written;
            if (!fat32_read_cluster(current_cluster, cluster_buffer)) {
                mem::heap::free(cluster_buffer);
                return false;
            }
        }
        
        mem::memcpy(cluster_buffer, data + bytes_written, bytes_to_copy);
        
        if (!fat32_write_cluster(current_cluster, cluster_buffer)) {
            mem::heap::free(cluster_buffer);
            return false;
        }
        
        bytes_written += bytes_to_copy;
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    
    mem::heap::free(cluster_buffer);
    return bytes_written == data_size;
}
