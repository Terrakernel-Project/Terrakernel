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
int64_t read(uint64_t lba, uint64_t c, uint8_t* buffer, size_t len);
int64_t write(uint64_t lba, uint64_t c, const uint8_t* data, size_t len);

disk_stats disk_get_stats();

}

#endif
