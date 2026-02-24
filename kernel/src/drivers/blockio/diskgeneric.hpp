#ifndef DISKGENERIC_HPP
#define DISKGENERIC_HPP 1

#include <cstdint>
#include <cstddef>
#include <pcie/pcie.hpp>

struct disk_stats {
	uint64_t sectors_written;
	uint64_t write_ops;
	uint64_t sectors_read;
	uint64_t read_ops;
	uint64_t errors;
};

#define DISKGENERIC_MEDIA_TYPE_AHCI_SATA 0
#define DISKGENERIC_MEDIA_TYPE_NVME      1

struct disk_info {
	uint64_t num_sectors;
	// (20971520*512)/(1024^3) = 10 GiB, where (n*512)/(1024^3) where n is sector count is the size in GiB
	uint64_t size_gib;
	uint64_t size_mib;
	uint64_t size_kib;
	uint64_t size_bytes;

	char name[16];

	int media_type;

	bool boot_disk;

	int disk_sn; // disk serial number
};

struct disk_driver {
	const char* name;
	bool (*init)(pcie_device* dev, disk_driver* driver);
	int64_t (*read)(int disk_sn, uint64_t lba, uint64_t c, uint8_t* buffer, size_t len);
	int64_t (*write)(int disk_sn, uint64_t lba, uint64_t c, const uint8_t* data, size_t len);
	int (*get_disk_count)();
	bool (*get_disk_info)(int disk_sn, disk_info* info);
};

namespace drivers::blockio::diskgeneric {

void initialise();
int64_t raw_read(int disk_sn, uint64_t lba, uint64_t c, uint8_t* buffer, size_t len);
int64_t raw_write(int disk_sn, uint64_t lba, uint64_t c, const uint8_t* data, size_t len);

int get_disk_count();
bool get_disk_info(int disk_sn, disk_info* info);

disk_stats disk_get_stats();

int get_boot_disk_sn();

namespace partitions {

void initialise();

uint64_t get_part_offset(int part_sn, uint64_t sect);
int get_num_parts();
int get_esp_part_sn_intrnl();

}

// partition stuff, uses a partition serial number
int64_t read(int disk_sn, int part_sn, uint64_t lba, uint64_t c, uint8_t* buffer, size_t len);
int64_t write(int disk_sn, int part_sn, uint64_t lba, uint64_t c, const uint8_t* data, size_t len);

int get_esp_part_sn();

int get_disk_count();
bool get_disk_info(int disk_sn, disk_info* info);

}

#endif
