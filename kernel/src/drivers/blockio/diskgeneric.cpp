#include "diskgeneric.hpp"
#include <panic.hpp>
#include <mem/mem.hpp>
#include "ahci/ahci.hpp"
#include "nvme/nvme.hpp"

static disk_driver* driver;
static pcie_device* device;

struct disk_id {
	uint16_t vendor;
	uint16_t device;
};

#define MAX_DISKS 44

disk_id disk_ids[MAX_DISKS] = {
    {0x8086, 0x2681}, // ICH6
    {0x8086, 0x2821}, // ICH8
    {0x8086, 0x2922}, // ICH9 (QEMU Q35)
    {0x8086, 0x2829}, // ICH8M
    {0x8086, 0x1C02}, // 6 Series/C200
    {0x8086, 0x1C03}, // 6 Series/C200
    {0x8086, 0x1E02}, // 7 Series/C210
    {0x8086, 0x1E03}, // 7 Series/C210
    {0x8086, 0x8C02}, // 8 Series/C220
    {0x8086, 0x8C03}, // 8 Series/C220
    {0x8086, 0x9C02}, // 8 Series Mobile
    {0x8086, 0x9C03}, // 8 Series Mobile
    {0x8086, 0xA102}, // Sunrise Point-H (100 Series)
    {0x8086, 0xA103}, // Sunrise Point-H
    {0x8086, 0x9D03}, // Sunrise Point-LP Mobile
    {0x8086, 0xA352}, // Cannon Lake
    {0x8086, 0x02D3}, // Comet Lake
    {0x1022, 0x7801}, // FCH SATA
    {0x1022, 0x7804}, // FCH SATA
    {0x1002, 0x4390}, // SB7x0/SB8x0/SB9x0
    {0x1002, 0x4391}, // SB7x0/SB8x0/SB9x0
    {0x1002, 0x4394}, // SB7x0/SB8x0/SB9x0
    {0x1106, 0x3349}, // VT8251
    {0x1106, 0x6287}, // VIA SATA
    {0x10DE, 0x0554}, // MCP67
    {0x10DE, 0x07F4}, // MCP73
    {0x10DE, 0x0AD4}, // MCP77
    {0x10DE, 0x0AB8}, // MCP79
    {0x10DE, 0x0D88}, // MCP89
    {0x1B4B, 0x9123}, // 88SE9123
    {0x1B4B, 0x9128}, // 88SE9128
    {0x1B4B, 0x9172}, // 88SE9172
    {0x1B4B, 0x9192}, // 88SE9192
    {0x1B4B, 0x91A0}, // 88SE912x
    {0x197B, 0x2360}, // JMB360
    {0x197B, 0x2361}, // JMB361
    {0x197B, 0x2362}, // JMB362
    {0x197B, 0x2363}, // JMB363
    {0x1B21, 0x0612}, // ASM1061
    {0x1B21, 0x0622}, // ASM1062
    {0x1B21, 0x0624}, // ASM1062+JMB575
    {0x15AD, 0x07E0}, // VMware SATA
    {0xFFFF, 0xFFFF}, // INVALID
};

void (*init_disk[MAX_DISKS])(pcie_device*,disk_driver*) = {
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	ahci_init,
	nullptr,
};

static disk_stats stats;

namespace drivers::blockio::diskgeneric {

void initialise() {
	int idx;
	for (int i = 0; i < MAX_DISKS; i++) {
		idx = i;
		device = pcie::get_device_vendor_id(disk_ids[i].vendor, disk_ids[i].device);
		if (device) break;
	}

	if (!device) {
		panic("no disk controller present or no supported disk controller");
	}

	driver = (disk_driver*)mem::heap::malloc(sizeof(disk_driver));

	init_disk[idx](device, driver);
}

int64_t raw_read(uint64_t lba, uint64_t c, uint8_t* buffer, size_t len) {
	stats.sectors_read += c;
	stats.read_ops++;
	if (!driver) {
		stats.errors++;
		return -1;
	}
	if (!driver->read) {
		stats.errors++;
		return -1;
	}
	int64_t ret = driver->read(lba, c, buffer, len);
	if (ret < 0) {
		stats.errors++;
		return ret;
	} else {
		return ret;
	}
}

int64_t raw_write(uint64_t lba, uint64_t c, const uint8_t* data, size_t len) {
	stats.sectors_written += c;
	stats.write_ops++;
	if (!driver) {
		stats.errors++;
		return -1;
	}
	if (!driver->write) {
		stats.errors++;
		return -1;
	}
	int64_t ret = driver->write(lba, c, data, len);
	if (ret < 0) {
		stats.errors++;
		return ret;
	} else {
		return ret;
	}
}

disk_stats disk_get_stats() {
	return stats;
}

int64_t read(int part_sn, uint64_t lba, uint64_t c, uint8_t* buffer, size_t len) {
	return raw_read(drivers::blockio::diskgeneric::partitions::get_part_offset(part_sn, lba), c, buffer, len);
}

int64_t write(int part_sn, uint64_t lba, uint64_t c, const uint8_t* data, size_t len) {
	return raw_write(drivers::blockio::diskgeneric::partitions::get_part_offset(part_sn, lba), c, data, len);
}

int get_esp_part_sn() {
	return drivers::blockio::diskgeneric::partitions::get_esp_part_sn_intrnl();	
}

}
