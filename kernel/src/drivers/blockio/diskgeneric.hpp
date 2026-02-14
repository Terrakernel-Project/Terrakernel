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

struct disk_driver {
	const char* name;
	bool (*init)(pcie_device* dev, disk_driver* driver);
	int64_t (*read)(uint64_t lba, uint64_t c, uint8_t* buffer, size_t len);
	int64_t (*write)(uint64_t lba, uint64_t c, const uint8_t* data, size_t len);
};

namespace drivers::blockio::diskgeneric {

void initialise();
int64_t raw_read(uint64_t lba, uint64_t c, uint8_t* buffer, size_t len);
int64_t raw_write(uint64_t lba, uint64_t c, const uint8_t* data, size_t len);

disk_stats disk_get_stats();

namespace partitions {

void initialise();

uint64_t get_part_offset(int part_sn, uint64_t sect);
int get_num_parts();
int get_esp_part_sn_intrnl();

}

// partition stuff, uses a partition serial number
int64_t read(int part_sn, uint64_t lba, uint64_t c, uint8_t* buffer, size_t len);
int64_t write(int part_sn, uint64_t lba, uint64_t c, const uint8_t* data, size_t len);

int get_esp_part_sn();

}

#endif
