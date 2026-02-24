#include "diskgeneric.hpp"
#include <panic.hpp>
#include <mem/mem.hpp>
#include <config.hpp>
#include <cstdio>
#include "ahci/ahci.hpp"
#include "nvme/nvme.hpp"

#ifdef CONFIG_DISK_GENERIC_VERBOSE
#	define DDPRINTF(fmt, ...) printf("[ %s ] " fmt, __PRETTY_FUNCTION__, ##__VA_ARGS__)
#else
#	define DDPRINTF(fmt, ...)
#endif

static disk_driver* driver; // TODO: allow multiple disks
static pcie_device* device;

struct disk_id {
	uint16_t vendor;
	uint16_t device;
	bool class_subclass;
	uint8_t class_code;
	uint8_t subclass_code;
};

#define MAX_DISKS 77

disk_id disk_ids[MAX_DISKS] = {
    // Intel AHCI
    {0x8086, 0x2681}, // ICH6
    {0x8086, 0x2821}, // ICH8
    {0x8086, 0x2922}, // ICH9 (QEMU Q35)
    {0x8086, 0x2829}, // ICH8M
    {0x8086, 0x2824}, // ICH8
    {0x8086, 0x2825}, // ICH8
    {0x8086, 0x3A02}, // ICH10
    {0x8086, 0x3A22}, // ICH10
    {0x8086, 0x1C02}, // 6 Series/C200
    {0x8086, 0x1C03}, // 6 Series/C200
    {0x8086, 0x1D02}, // C600/X79
    {0x8086, 0x1D04}, // C600/X79
    {0x8086, 0x1E02}, // 7 Series/C210
    {0x8086, 0x1E03}, // 7 Series/C210
    {0x8086, 0x8C02}, // 8 Series/C220
    {0x8086, 0x8C03}, // 8 Series/C220
    {0x8086, 0x9C02}, // 8 Series Mobile
    {0x8086, 0x9C03}, // 8 Series Mobile
    {0x8086, 0x8D02}, // C610/X99
    {0x8086, 0x8D04}, // C610/X99
    {0x8086, 0xA102}, // Sunrise Point-H (100 Series)
    {0x8086, 0xA103}, // Sunrise Point-H
    {0x8086, 0x9D03}, // Sunrise Point-LP Mobile
    {0x8086, 0xA282}, // 200 Series
    {0x8086, 0xA352}, // Cannon Lake
    {0x8086, 0xA353}, // Cannon Lake
    {0x8086, 0x02D3}, // Comet Lake
    {0x8086, 0x06D2}, // Comet Lake
    {0x8086, 0x43D2}, // Tiger Lake
    {0x8086, 0x7AE2}, // Alder Lake
    {0x8086, 0x51D3}, // Alder Lake-P
    {0x8086, 0x54D3}, // Alder Lake-N
    
    // AMD AHCI
    {0x1022, 0x7801}, // FCH SATA
    {0x1022, 0x7804}, // FCH SATA
    {0x1022, 0x7900}, // FCH SATA (Ryzen)
    {0x1022, 0x7901}, // FCH SATA (Ryzen)
    
    // ATI/AMD SB Series
    {0x1002, 0x4390}, // SB7x0/SB8x0/SB9x0
    {0x1002, 0x4391}, // SB7x0/SB8x0/SB9x0
    {0x1002, 0x4394}, // SB7x0/SB8x0/SB9x0
    {0x1002, 0x4395}, // SB7x0/SB8x0/SB9x0
    
    // VIA AHCI
    {0x1106, 0x3349}, // VT8251
    {0x1106, 0x6287}, // VIA SATA
    {0x1106, 0x9000}, // VIA SATA
    
    // NVIDIA AHCI
    {0x10DE, 0x0554}, // MCP67
    {0x10DE, 0x07F4}, // MCP73
    {0x10DE, 0x0AD4}, // MCP77
    {0x10DE, 0x0AB8}, // MCP79
    {0x10DE, 0x0D88}, // MCP89
    
    // Marvell AHCI
    {0x1B4B, 0x9123}, // 88SE9123
    {0x1B4B, 0x9128}, // 88SE9128
    {0x1B4B, 0x9130}, // 88SE9130
    {0x1B4B, 0x9170}, // 88SE9170
    {0x1B4B, 0x9172}, // 88SE9172
    {0x1B4B, 0x9178}, // 88SE9178
    {0x1B4B, 0x9182}, // 88SE9182
    {0x1B4B, 0x9192}, // 88SE9192
    {0x1B4B, 0x91A0}, // 88SE912x
    {0x1B4B, 0x9215}, // 88SE9215
    {0x1B4B, 0x9230}, // 88SE9230
    {0x1B4B, 0x9235}, // 88SE9235
    
    // JMicron AHCI
    {0x197B, 0x2360}, // JMB360
    {0x197B, 0x2361}, // JMB361
    {0x197B, 0x2362}, // JMB362
    {0x197B, 0x2363}, // JMB363
    {0x197B, 0x2365}, // JMB365
    {0x197B, 0x2366}, // JMB366
    
    // ASMedia AHCI
    {0x1B21, 0x0601}, // ASM1060
    {0x1B21, 0x0602}, // ASM1060
    {0x1B21, 0x0611}, // ASM1061
    {0x1B21, 0x0612}, // ASM1061
    {0x1B21, 0x0622}, // ASM1062
    {0x1B21, 0x0624}, // ASM1062+JMB575
    {0x1B21, 0x0625}, // ASM1064
    
    // Virtualization AHCI
    {0x15AD, 0x07E0}, // VMware SATA
    {0x1AF4, 0x1001}, // VirtIO Block

	// NVMe
    {0xFFFF, 0xFFFF, true, 0x01, 0x08}, // All NVMe devices (Class 01h, Subclass 08h)

    {0xFFFF, 0xFFFF}, // INVALID
};

void (*init_disk[MAX_DISKS])(pcie_device*, disk_driver*) = {
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
	nvme_init,
	nullptr,
};

static disk_stats stats;

namespace drivers::blockio::diskgeneric {

void initialise() {
	int idx;
	for (int i = 0; i < MAX_DISKS; i++) {
		idx = i;
		if (disk_ids[i].class_subclass) {
			device = pcie::get_device_class_code(disk_ids[i].class_code, disk_ids[i].subclass_code);
		} else {
			device = pcie::get_device_vendor_id(disk_ids[i].vendor, disk_ids[i].device);
		}
		
		if (device) {
			if (disk_ids[i].class_subclass) {
				DDPRINTF("Found device... Class code: %02X / Subclass code: %02X\n\r", disk_ids[i].class_code, disk_ids[i].subclass_code);
			} else {
				DDPRINTF("Found device... Vendor ID: %04X / Device ID: %04X\n\r", disk_ids[i].vendor, disk_ids[i].device);
			}
			break;
		}
	}

	if (!device) {
		panic("no disk controller present or no supported disk controller");
	}

	driver = (disk_driver*)mem::heap::malloc(sizeof(disk_driver));

	init_disk[idx](device, driver);
}

int64_t raw_read(int disk_sn, uint64_t lba, uint64_t c, uint8_t* buffer, size_t len) {
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
	int64_t ret = driver->read(disk_sn, lba, c, buffer, len);
	if (ret < 0) {
		stats.errors++;
		return ret;
	} else {
		return ret;
	}
}

int64_t raw_write(int disk_sn, uint64_t lba, uint64_t c, const uint8_t* data, size_t len) {
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
	int64_t ret = driver->write(disk_sn, lba, c, data, len);
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

int64_t read(int disk_sn, int part_sn, uint64_t lba, uint64_t c, uint8_t* buffer, size_t len) {
	return raw_read(disk_sn, drivers::blockio::diskgeneric::partitions::get_part_offset(part_sn, lba), c, buffer, len);
}

int64_t write(int disk_sn, int part_sn, uint64_t lba, uint64_t c, const uint8_t* data, size_t len) {
	return raw_write(disk_sn, drivers::blockio::diskgeneric::partitions::get_part_offset(part_sn, lba), c, data, len);
}

int get_esp_part_sn() {
	return drivers::blockio::diskgeneric::partitions::get_esp_part_sn_intrnl();
}

int get_disk_count() {
	if (!driver->get_disk_count) {
		stats.errors++;
		return -1;
	}

	return driver->get_disk_count();
}

bool get_disk_info(int disk_sn, disk_info* info) {
	if (!driver->get_disk_info) {
		stats.errors++;
		return false;
	}

	return driver->get_disk_info(disk_sn, info);
}

int boot_disk = -1;

int get_boot_disk_sn() {
	if (boot_disk != -1) return boot_disk;
		
	disk_info info;
	
	for (int i = 0; i < get_disk_count(); i++) {
		if (get_disk_info(i, &info)) {
			if (info.boot_disk) {
				boot_disk = i;
				break;
			}
		}
	}

	return boot_disk;
}

}
