#include "ahci.hpp"
#include <cstdio>
#include <mem/mem.hpp>
#include <cstring>
#include <config.hpp>
#include <drivers/timers/apic/apic.hpp>

#ifdef CONFIG_AHCI_VERBOSE
#	define ADPRINTF(fmt, ...) printf("[ %s ] " fmt, __PRETTY_FUNCTION__, ##__VA_ARGS__)
#else
#	define ADPRINTF(fmt, ...)
#endif

struct ahci_mbr_struct {
	uint8_t mbr_bootstrap[440];
	uint32_t unique_disk_id;
	uint16_t reserved;
	uint8_t rsv[64]; // partitions are here, but who cares we are just checking if boot disk
	uint16_t mbr_sig;
} __attribute__((packed));

struct ahci_identify_struc {
    uint16_t general_config;
    uint16_t obsolete1[9];

    char     serial_number[20];

    uint16_t obsolete2[3];

    char     firmware_revision[8];
    char     model_number[40];

    uint16_t max_sectors_per_interrupt;

    uint16_t trusted_computing;

    uint16_t capabilities1;
    uint16_t capabilities2;

    uint16_t obsolete3[2];

    uint16_t validity_flags;

    uint16_t obsolete4[5];

    uint16_t multi_sector_settings;

    uint32_t lba28_total_sectors;

    uint16_t obsolete5;

    uint16_t multiword_dma;
    uint16_t pio_modes;

    uint16_t min_mwdma_cycle_time;
    uint16_t rec_mwdma_cycle_time;
    uint16_t min_pio_cycle_time;
    uint16_t min_pio_cycle_time_iordy;

    uint16_t reserved1[6];

    uint16_t queue_depth;

    uint16_t sata_capabilities;
    uint16_t sata_reserved;
    uint16_t sata_features_supported;
    uint16_t sata_features_enabled;

    uint16_t major_version;
    uint16_t minor_version;

    uint16_t command_set1;
    uint16_t command_set2;
    uint16_t command_set3;
    uint16_t command_set_enabled1;
    uint16_t command_set_enabled2;
    uint16_t command_set_default;

    uint16_t ultra_dma_modes;

    uint16_t reserved2[4];

    uint16_t hw_reset_result;

    uint16_t reserved3[5];

    uint16_t lba48_total_sectors[4];

    uint16_t reserved4[23];

    uint16_t removable_media_status;
    uint16_t security_status;

    uint16_t reserved5[127];
};

struct internal_port {
	HBA_PORT* hba_port;
	uint8_t port_id;
	char port_name[16]; // preferably follow sata_a, sata_b, sata_c, ...
	bool online;

	ahci_identify_struc port_info;
	ahci_mbr_struct mbr;
};

internal_port driver_ports[32];
int installed_ports;
int counter = 0;
const char* port_name_reference_table = "abcdefghijklmnopqrstuvwxyzABCDEF";

void get_next_port_name(char* name) {
	if (!name) return;
	if (counter >= 32) return;
	char port_name[7] = "sata_\0";
	port_name[5] = port_name_reference_table[counter++];
	strncpy(name, port_name, 7);
}

internal_port* implement_port(HBA_PORT* port) {
	driver_ports[installed_ports].hba_port = port;
	driver_ports[installed_ports].port_id = installed_ports;
	get_next_port_name(driver_ports[installed_ports].port_name);
	driver_ports[installed_ports].online = false;
	
	return &driver_ports[installed_ports++];
}

void stop_port(HBA_PORT* port) {
	if (!(port->cmd & (1 << 0))) {
		return;
	}
	
	port->cmd &= ~(1 << 0);
	port->cmd &= ~(1 << 4);
	
	uint64_t timeout = 100000;
	while ((port->cmd & ((1 << 15) | (1 << 14))) && timeout > 0) timeout--;
}

void start_port(HBA_PORT* port) {
	while (port->cmd & (1 << 15));
	
	port->cmd |= (1 << 4);
	port->cmd |= (1 << 0);
}

#define SATA_SIG_ATAPI 0xEB140101
#define SATA_SIG_ATA 0x00000101
#define SATA_SIG_SEMB 0xC33C0101
#define SATA_SIG_PM 0x96690101

bool supported_port_type(HBA_PORT* hba_port) {
	switch (hba_port->sig) {
		case SATA_SIG_ATAPI:
			ADPRINTF("Port is a SATAPI port, unsupported\n\r");
			return false;
		case SATA_SIG_ATA:
			ADPRINTF("Port is a SATA port\n\r");
			return true;
		case SATA_SIG_SEMB:
			ADPRINTF("Port is a SEMB port, unsupported\n\r");
			return false;
		case SATA_SIG_PM:
			ADPRINTF("Port is a PM port, unsupported\n\r");
			return false;
	}

	return false;
}

bool ahci_identify(HBA_PORT* hba_port, uint16_t* identify_buffer);
int64_t ahci_read(int disk_sn, uint64_t lba, uint64_t count, uint8_t* buffer, size_t len);
int64_t ahci_write(int disk_sn, uint64_t lba, uint64_t count, const uint8_t* data, size_t len);
int ahci_disk_count();
bool ahci_disk_info(int disk_sn, disk_info* info);

void setup_port(internal_port* port) {
	ADPRINTF("Setting up port %s\n\r", port->port_name);

	HBA_PORT* hba_port = port->hba_port;
	
	stop_port(hba_port);

	ADPRINTF("Allocating memory for CLB/FB/CTBA for port %s\n\r", port->port_name);
	
	uint64_t clb_virt = (uint64_t)mem::vmm::valloc(1);
	uint64_t clb_phys = (uint64_t)mem::vmm::va_to_pa(clb_virt);
	
	uint64_t fb_virt = (uint64_t)mem::vmm::valloc(1);
	uint64_t fb_phys = (uint64_t)mem::vmm::va_to_pa(fb_virt);
	
	uint64_t ctba_virt = (uint64_t)mem::vmm::valloc(2);
	uint64_t ctba_phys = (uint64_t)mem::vmm::va_to_pa(ctba_virt);
	
	mem::memset((void*)clb_virt, 0, 4096);
	mem::memset((void*)fb_virt, 0, 4096);
	mem::memset((void*)ctba_virt, 0, 8192);
	
	hba_port->clb = (uint32_t)(clb_phys & 0xFFFFFFFF);
	hba_port->clbu = (uint32_t)(clb_phys >> 32);
	hba_port->fb = (uint32_t)(fb_phys & 0xFFFFFFFF);
	hba_port->fbu = (uint32_t)(fb_phys >> 32);
	ADPRINTF("Registered CLB=0x%08x%08x FB=0x%08x%08x\n\r", 
	         hba_port->clbu, hba_port->clb, hba_port->fbu, hba_port->fb);
	
	HBA_CMD_HEADER* cmd_list = (HBA_CMD_HEADER*)clb_virt;
	for (int i = 0; i < 32; i++) {
		cmd_list[i].prdtl = 8;
		uint64_t ct_phys = ctba_phys + (i * 256);
		cmd_list[i].ctba = (uint32_t)(ct_phys & 0xFFFFFFFF);
		cmd_list[i].ctbau = (uint32_t)(ct_phys >> 32);
	}
	ADPRINTF("Prepared command list for port %s\n\r", port->port_name);
	
	hba_port->is = 0xFFFFFFFF;
	hba_port->serr = 0xFFFFFFFF;
	
	hba_port->cmd |= (1 << 4);
	
	uint64_t timeout = 100000;
	while (!(hba_port->cmd & (1 << 4)) && timeout > 0) timeout--;
	
	ADPRINTF("Performing COMRESET on port %s\n\r", port->port_name);
	
	uint32_t sctl = hba_port->sctl;
	sctl = (sctl & ~0xF) | 0x1;
	hba_port->sctl = sctl;
	
	for (volatile int i = 0; i < 100000; i++);
	
	hba_port->sctl = sctl & ~0xF;
	
	ADPRINTF("Waiting for device detection on port %s\n\r", port->port_name);
	timeout = 1000000;
	while (timeout > 0) {
		uint32_t ssts = hba_port->ssts;
		uint8_t det = ssts & 0xF;
		uint8_t ipm = (ssts >> 8) & 0xF;
		
		if (det == 0x3 && ipm == 0x1) {
			port->online = true;
			ADPRINTF("Port %s is online - DET=0x%x IPM=0x%x\n\r", 
			         port->port_name, det, ipm);
			break;
		}
		
		if (det == 0x0) {
			break;
		}
		
		for (volatile int i = 0; i < 10000; i++);
		timeout--;
	}
	
	if (!port->online) {
		uint32_t ssts = hba_port->ssts;
		ADPRINTF("Port %s is offline - DET=0x%x IPM=0x%x SSTS=0x%08x SERR=0x%08x\n\r", 
		         port->port_name, ssts & 0xF, (ssts >> 8) & 0xF, ssts, hba_port->serr);
		return;
	}
	
	uint32_t sig = hba_port->sig;
	ADPRINTF("Port %s signature: 0x%08x\n\r", port->port_name, sig);
	if (sig == 0xEB140101) {
		ADPRINTF("  -> ATAPI device (CD-ROM)\n\r");
	} else if (sig == 0x00000101) {
		ADPRINTF("  -> SATA drive\n\r");
	} else {
		ADPRINTF("  -> Unknown device type\n\r");
	}
	
	hba_port->cmd |= (1 << 2);
	hba_port->cmd |= (1 << 1);
	
	ADPRINTF("Starting port %s\n\r", port->port_name);
	start_port(hba_port);

	ahci_identify(hba_port, (uint16_t*)&port->port_info); // identify disk
	ahci_read(port->port_id, 0, 1, (uint8_t*)&port->mbr, 512); // read mbr

	ADPRINTF("===============================\n\r\n\r");
}

HBA_MEM* abar;

void error_dump(FIS_REG_H2D* fis, HBA_CMD_TBL* cmd_tbl, HBA_CMD_HEADER* cmd_header) {
	ADPRINTF("FIS dump:\n\r");
	ADPRINTF("  fis_type=0x%02x c=%d command=0x%02x\n\r", fis->fis_type, fis->c, fis->command);
	ADPRINTF("  device=0x%02x\n\r", fis->device);
	ADPRINTF("  lba=0x%02x%02x%02x%02x%02x%02x\n\r", fis->lba5, fis->lba4, fis->lba3, fis->lba2, fis->lba1, fis->lba0);
	ADPRINTF("  count=0x%04x\n\r", (fis->counth << 8) | fis->countl);
	
	ADPRINTF("PRDT dump:\n\r");
	ADPRINTF("  dba=0x%08x%08x\n\r", cmd_tbl->prdt_entry[0].dbau, cmd_tbl->prdt_entry[0].dba);
	ADPRINTF("  dbc=0x%08x (bytes=%u)\n\r", cmd_tbl->prdt_entry[0].dbc, cmd_tbl->prdt_entry[0].dbc + 1);
	
	ADPRINTF("Command header dump:\n\r");
	ADPRINTF("  cfl=%d w=%d prdtl=%d\n\r", cmd_header->cfl, cmd_header->w, cmd_header->prdtl);
}

void ahci_init(pcie_device* dev, disk_driver* driver) {
	ADPRINTF("Initialising AHCI disk controller\n\r");
	
	driver->name = "AHCI";
	driver->read = ahci_read;
	driver->write = ahci_write;
	driver->get_disk_count = ahci_disk_count;
	driver->get_disk_info = ahci_disk_info;
	
	pcie::enable_bus_mastering(dev);
	
	uint64_t abar_phys = dev->bars[5] & ~0xFULL;
	abar = (HBA_MEM*)mem::vmm::pa_to_va((uint64_t)abar_phys);
	mem::vmm::mmap((void*)abar_phys, abar, (sizeof(HBA_MEM) + 0xFFF) / 0x1000, PAGE_PRESENT | PAGE_RW | PAGE_PCD);
	
	abar->ghc |= (1 << 31);
	
	uint32_t pi = abar->pi;
	
	for (int i = 0; i < 32; i++) {
		if (pi & (1 << i)) {
			if (supported_port_type(&abar->ports[i])) {
				internal_port* port = implement_port(&abar->ports[i]);
				setup_port(port);
			} else {
				ADPRINTF("Skipping port %d due to not being supported...\n\r", i);
			}
			continue;
		} else {
			continue;
		}
	}
}

int find_cmdslot(HBA_PORT* port) {
	uint32_t slots = (port->sact | port->ci);
	for (int i = 0; i < 32; i++) {
		if ((slots & (1 << i)) == 0) {
			return i;
		}
	}
	return -1;
}

bool ahci_identify(HBA_PORT* hba_port, uint16_t* identify_buffer) {
	ADPRINTF("Sending IDENTIFY DEVICE command\n\r");

	void* intrnl_buf_phys = mem::pmm::palloc(1);
	void* intrnl_buf = (void*)mem::vmm::pa_to_va((uint64_t)intrnl_buf_phys);
	mem::memset(intrnl_buf, 0, 4096);
	
	hba_port->serr = 0xFFFFFFFF;
	hba_port->is = 0xFFFFFFFF;
	
	uint64_t timeout = 100000;
	while (timeout > 0) {
		uint32_t tfd = hba_port->tfd;
		uint8_t status = tfd & 0xFF;
		if (!(status & 0x80) && !(status & 0x08)) {
			break;
		}
		timeout--;
	}
	
	if (timeout == 0) {
		ADPRINTF("Device not ready for IDENTIFY\n\r");
		mem::pmm::free(intrnl_buf_phys, 1);
		return false;
	}
	
	int slot = find_cmdslot(hba_port);
	if (slot == -1) {
		ADPRINTF("No command slot available\n\r");
		mem::pmm::free(intrnl_buf_phys, 1);
		return false;
	}
	
	uint64_t clb_phys = ((uint64_t)hba_port->clbu << 32) | hba_port->clb;
	HBA_CMD_HEADER* cmd_header = (HBA_CMD_HEADER*)mem::vmm::pa_to_va((uint64_t)clb_phys);
	cmd_header += slot;
	
	cmd_header->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
	cmd_header->w = 0;
	cmd_header->c = 0;
	cmd_header->p = 0;
	cmd_header->prdtl = 1;
	cmd_header->prdbc = 0;
	
	uint64_t ctba_phys = ((uint64_t)cmd_header->ctbau << 32) | cmd_header->ctba;
	HBA_CMD_TBL* cmd_tbl = (HBA_CMD_TBL*)mem::vmm::pa_to_va((uint64_t)ctba_phys);
	mem::memset(cmd_tbl, 0, sizeof(HBA_CMD_TBL) + sizeof(HBA_PRDT_ENTRY) * 7);
	
	uint64_t buffer_phys = (uint64_t)intrnl_buf_phys;
	cmd_tbl->prdt_entry[0].dba = (uint32_t)(buffer_phys & 0xFFFFFFFF);
	cmd_tbl->prdt_entry[0].dbau = (uint32_t)(buffer_phys >> 32);
	cmd_tbl->prdt_entry[0].dbc = 512 - 1;
	cmd_tbl->prdt_entry[0].i = 0;
	
	FIS_REG_H2D* fis = (FIS_REG_H2D*)(&cmd_tbl->cfis);
	mem::memset(fis, 0, sizeof(FIS_REG_H2D));
	
	fis->fis_type = FIS_TYPE_REG_H2D;
	fis->c = 1;
	fis->command = 0xEC;
	fis->device = 0;
	fis->countl = 1;
	fis->counth = 0;
	
	hba_port->is = 0xFFFFFFFF;
	
	ADPRINTF("Issuing IDENTIFY on slot %d\n\r", slot);
	hba_port->ci = 1 << slot;

	drivers::timers::apic::sleep_ms(5);
	if (hba_port->ci & (1 << slot) != 0) {
		ADPRINTF("Command timed out\n\r");
		return -1;
	}

	if (hba_port->is & (1 << 30)) {
		ADPRINTF("Task file error:\n\r");
		ADPRINTF("  IS=0x%08x\n\r", hba_port->is);
		ADPRINTF("  SERR=0x%08x\n\r", hba_port->serr);
		ADPRINTF("  TFD=0x%08x\n\r", hba_port->tfd);
		ADPRINTF("  SSTS=0x%08x\n\r", hba_port->ssts);
				
		error_dump(fis, cmd_tbl, cmd_header);
							         
		hba_port->is = hba_port->is;
		hba_port->serr = hba_port->serr;
		mem::pmm::free(intrnl_buf_phys, 1);
		return false;
	}
	
	uint16_t* id_data = (uint16_t*)intrnl_buf;
	uint64_t sectors = ((uint64_t)id_data[103] << 48) |
	                   ((uint64_t)id_data[102] << 32) |
	                   ((uint64_t)id_data[101] << 16) |
	                   ((uint64_t)id_data[100]);
	
	if (sectors == 0) {
		sectors = ((uint32_t)id_data[61] << 16) | id_data[60];
	}
	
	ADPRINTF("Device capacity: %lu sectors (%lu MB)\n\r", sectors, (sectors * 512) / (1024 * 1024));

	mem::memcpy(identify_buffer, intrnl_buf, 512);

	mem::pmm::free(intrnl_buf_phys, 1);
	
	return true;
}

int64_t ahci_read(int disk_sn, uint64_t lba, uint64_t count, uint8_t* buffer, size_t len) {
	if (len < (count * 512)) {
		return -1;
	}

	if (count == 0 || count > 65536) {
		return -1;
	}

	if (0 <= disk_sn && disk_sn < installed_ports) {} else {
		return -1;
	}

	size_t dma_pages = ((count * 512) + 0xFFF) / 0x1000;
	void* intrnl_buf_phys = mem::pmm::palloc(dma_pages);
	void* intrnl_buf = (void*)mem::vmm::pa_to_va((uint64_t)intrnl_buf_phys);
	mem::memset(intrnl_buf, 0, dma_pages * 0x1000);

	ADPRINTF("Reading from port %s: LBA=%lu count=%lu\n\r", driver_ports[disk_sn].port_name, lba, count);
	
	if (installed_ports == 0) {
		ADPRINTF("No ports available\n\r");
		mem::pmm::free(intrnl_buf_phys, (len + 0xFFF) / 0x1000);
		return -1;
	}
	
	internal_port* port = &driver_ports[disk_sn];
	if (!port->online) {
		ADPRINTF("Port is offline\n\r");
		mem::pmm::free(intrnl_buf_phys, (len + 0xFFF) / 0x1000);
		return -1;
	}
	
	HBA_PORT* hba_port = port->hba_port;

	hba_port->serr = 0xFFFFFFFF;
	hba_port->is = 0xFFFFFFFF;
	
	uint64_t timeout = 1000000;
	while (timeout > 0) {
		uint32_t tfd = hba_port->tfd;
		uint8_t status = tfd & 0xFF;
		
		if (!(status & 0x80) && !(status & 0x08)) {
			break;
		}
		timeout--;
	}
	
	if (timeout == 0) {
		ADPRINTF("Device not ready (TFD=0x%08x)\n\r", hba_port->tfd);
		mem::pmm::free(intrnl_buf_phys, (len + 0xFFF) / 0x1000);
		return -1;
	}
	
	int slot = find_cmdslot(hba_port);
	if (slot == -1) {
		ADPRINTF("No command slot available\n\r");
		mem::pmm::free(intrnl_buf_phys, (len + 0xFFF) / 0x1000);
		return -1;
	}
	ADPRINTF("Using command slot %d\n\r", slot);
	
	uint64_t clb_phys = ((uint64_t)hba_port->clbu << 32) | hba_port->clb;
	HBA_CMD_HEADER* cmd_header = (HBA_CMD_HEADER*)mem::vmm::pa_to_va((uint64_t)clb_phys);
	cmd_header += slot;
	
	cmd_header->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
	cmd_header->w = 0;
	cmd_header->c = 0;
	cmd_header->p = 0;
	cmd_header->prdtl = 1;
	cmd_header->prdbc = 0;
	
	uint64_t ctba_phys = ((uint64_t)cmd_header->ctbau << 32) | cmd_header->ctba;
	HBA_CMD_TBL* cmd_tbl = (HBA_CMD_TBL*)mem::vmm::pa_to_va((uint64_t)ctba_phys);
	mem::memset(cmd_tbl, 0, sizeof(HBA_CMD_TBL) + sizeof(HBA_PRDT_ENTRY) * 7);
	
	uint64_t buffer_phys = (uint64_t)intrnl_buf_phys;
	ADPRINTF("Buffer virt=%p phys=0x%lx\n\r", intrnl_buf, buffer_phys);
	
	cmd_tbl->prdt_entry[0].dba = (uint32_t)(buffer_phys & 0xFFFFFFFF);
	cmd_tbl->prdt_entry[0].dbau = (uint32_t)(buffer_phys >> 32);
	cmd_tbl->prdt_entry[0].dbc = (count * 512) - 1;
	cmd_tbl->prdt_entry[0].i = 0;
	
	FIS_REG_H2D* fis = (FIS_REG_H2D*)(&cmd_tbl->cfis);
	mem::memset(fis, 0, sizeof(FIS_REG_H2D));
	
	fis->fis_type = FIS_TYPE_REG_H2D;
	fis->c = 1;
	fis->command = 0x25;
	
	fis->lba0 = lba & 0xFF;
	fis->lba1 = (lba >> 8) & 0xFF;
	fis->lba2 = (lba >> 16) & 0xFF;
	fis->lba3 = (lba >> 24) & 0xFF;
	fis->lba4 = (lba >> 32) & 0xFF;
	fis->lba5 = (lba >> 40) & 0xFF;
	
	fis->device = 0x40;
	
	fis->countl = count & 0xFF;
	fis->counth = (count >> 8) & 0xFF;
	
	hba_port->is = 0xFFFFFFFF;
	
	ADPRINTF("Pre-command state: SSTS=0x%08x TFD=0x%08x SERR=0x%08x\n\r", 
	         hba_port->ssts, hba_port->tfd, hba_port->serr);
	
	ADPRINTF("Issuing READ command on slot %d\n\r", slot);
	hba_port->ci = 1 << slot;

	drivers::timers::apic::sleep_ms(5);
	if (hba_port->ci & (1 << slot) != 0) {
		ADPRINTF("Command timed out\n\r");
		return -1;
	}

	if (hba_port->is & (1 << 30)) {
		ADPRINTF("Task file error:\n\r");
		ADPRINTF("  IS=0x%08x\n\r", hba_port->is);
		ADPRINTF("  SERR=0x%08x\n\r", hba_port->serr);
		ADPRINTF("  TFD=0x%08x\n\r", hba_port->tfd);
		ADPRINTF("  SSTS=0x%08x\n\r", hba_port->ssts);
				
		error_dump(fis, cmd_tbl, cmd_header);
							         
		hba_port->is = hba_port->is;
		hba_port->serr = hba_port->serr;
		mem::pmm::free(intrnl_buf_phys, 1);
		return false;
	}

	mem::memcpy(buffer, intrnl_buf, count * 512);

	mem::pmm::free(intrnl_buf_phys, (len + 0xFFF) / 0x1000);
	
	ADPRINTF("Read completed, transferred %u bytes\n\r", cmd_header->prdbc);
	return count * 512;
}

int64_t ahci_write(int disk_sn, uint64_t lba, uint64_t count, const uint8_t* data, size_t len) {
	if (len < (count * 512)) {
		return -1;
	}

	if (count == 0 || count > 65536) {
		return -1;
	}

	if (0 <= disk_sn && disk_sn < installed_ports) {} else {
		return -1;
	}

	size_t dma_pages = ((count * 512) + 0xFFF) / 0x1000;
	void* intrnl_buf_phys = mem::pmm::palloc(dma_pages);
	void* intrnl_buf = (void*)mem::vmm::pa_to_va((uint64_t)intrnl_buf_phys);

	mem::memcpy(intrnl_buf, data, count * 512);

	ADPRINTF("Writing to port %s: LBA=%lu count=%lu\n\r", driver_ports[disk_sn].port_name, lba, count);
	
	if (installed_ports == 0) {
		ADPRINTF("No ports available\n\r");
		mem::pmm::free(intrnl_buf_phys, (len + 0xFFF) / 0x1000);
		return -1;
	}
	
	internal_port* port = &driver_ports[disk_sn];
	if (!port->online) {
		ADPRINTF("Port is offline\n\r");
		mem::pmm::free(intrnl_buf_phys, (len + 0xFFF) / 0x1000);
		return -1;
	}
	
	HBA_PORT* hba_port = port->hba_port;
	
	hba_port->serr = 0xFFFFFFFF;
	hba_port->is = 0xFFFFFFFF;
	
	uint64_t timeout = 1000000;
	while (timeout > 0) {
		uint32_t tfd = hba_port->tfd;
		uint8_t status = tfd & 0xFF;
		
		if (!(status & 0x80) && !(status & 0x08)) {
			break;
		}
		timeout--;
	}
	
	if (timeout == 0) {
		ADPRINTF("Device not ready (TFD=0x%08x)\n\r", hba_port->tfd);
		mem::pmm::free(intrnl_buf_phys, (len + 0xFFF) / 0x1000);
		return -1;
	}
	
	int slot = find_cmdslot(hba_port);
	if (slot == -1) {
		ADPRINTF("No command slot available\n\r");
		mem::pmm::free(intrnl_buf_phys, (len + 0xFFF) / 0x1000);
		return -1;
	}
	ADPRINTF("Using command slot %d\n\r", slot);
	
	uint64_t clb_phys = ((uint64_t)hba_port->clbu << 32) | hba_port->clb;
	HBA_CMD_HEADER* cmd_header = (HBA_CMD_HEADER*)mem::vmm::pa_to_va((uint64_t)clb_phys);
	cmd_header += slot;
	
	cmd_header->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
	cmd_header->w = 1;
	cmd_header->c = 0;
	cmd_header->p = 0;
	cmd_header->prdtl = 1;
	cmd_header->prdbc = 0;
	
	uint64_t ctba_phys = ((uint64_t)cmd_header->ctbau << 32) | cmd_header->ctba;
	HBA_CMD_TBL* cmd_tbl = (HBA_CMD_TBL*)mem::vmm::pa_to_va((uint64_t)ctba_phys);
	mem::memset(cmd_tbl, 0, sizeof(HBA_CMD_TBL) + sizeof(HBA_PRDT_ENTRY) * 7);
	
	uint64_t buffer_phys = (uint64_t)intrnl_buf_phys;
	ADPRINTF("Buffer virt=%p phys=0x%lx\n\r", intrnl_buf, buffer_phys);
	
	cmd_tbl->prdt_entry[0].dba = (uint32_t)(buffer_phys & 0xFFFFFFFF);
	cmd_tbl->prdt_entry[0].dbau = (uint32_t)(buffer_phys >> 32);
	cmd_tbl->prdt_entry[0].dbc = (count * 512) - 1;
	cmd_tbl->prdt_entry[0].i = 0;
	
	FIS_REG_H2D* fis = (FIS_REG_H2D*)(&cmd_tbl->cfis);
	mem::memset(fis, 0, sizeof(FIS_REG_H2D));
	
	fis->fis_type = FIS_TYPE_REG_H2D;
	fis->c = 1;
	fis->command = 0x35;
	
	fis->lba0 = lba & 0xFF;
	fis->lba1 = (lba >> 8) & 0xFF;
	fis->lba2 = (lba >> 16) & 0xFF;
	fis->lba3 = (lba >> 24) & 0xFF;
	fis->lba4 = (lba >> 32) & 0xFF;
	fis->lba5 = (lba >> 40) & 0xFF;
	
	fis->device = 0x40;
	
	fis->countl = count & 0xFF;
	fis->counth = (count >> 8) & 0xFF;
	
	hba_port->is = 0xFFFFFFFF;
	
	ADPRINTF("Pre-command state: SSTS=0x%08x TFD=0x%08x SERR=0x%08x\n\r", 
	         hba_port->ssts, hba_port->tfd, hba_port->serr);
	
	ADPRINTF("Issuing WRITE command on slot %d\n\r", slot);
	hba_port->ci = 1 << slot;

	drivers::timers::apic::sleep_ms(5);
	if (hba_port->ci & (1 << slot) != 0) {
		ADPRINTF("Command timed out\n\r");
		return -1;
	}

	if (hba_port->is & (1 << 30)) {
		ADPRINTF("Task file error:\n\r");
		ADPRINTF("  IS=0x%08x\n\r", hba_port->is);
		ADPRINTF("  SERR=0x%08x\n\r", hba_port->serr);
		ADPRINTF("  TFD=0x%08x\n\r", hba_port->tfd);
		ADPRINTF("  SSTS=0x%08x\n\r", hba_port->ssts);
				
		error_dump(fis, cmd_tbl, cmd_header);
							         
		hba_port->is = hba_port->is;
		hba_port->serr = hba_port->serr;
		mem::pmm::free(intrnl_buf_phys, 1);
		return false;
	}
	
	mem::pmm::free(intrnl_buf_phys, (len + 0xFFF) / 0x1000);
	
	ADPRINTF("Write completed, transferred %u bytes\n\r", cmd_header->prdbc);
	return count * 512;
}

int ahci_disk_count() {
	return installed_ports;
}

#include <limine.h>

extern volatile limine_executable_file_request executable_file_request;

limine_file* exec_file = nullptr;

bool ahci_disk_info(int disk_sn, disk_info* info) {
	if (!exec_file && executable_file_request.response) {
		exec_file = executable_file_request.response->executable_file;
	}

	if (!info) return false;

	if (0 <= disk_sn && disk_sn < installed_ports) {} else {
		return false;
	}

	ahci_identify_struc identify_struc_ins;
	ahci_identify_struc* identify_struc = &identify_struc_ins; // ptr
	
	ahci_identify(driver_ports[disk_sn].hba_port, (uint16_t*)identify_struc);

	uint64_t lba48 =
		((uint64_t)identify_struc->lba48_total_sectors[3] << 48) |
	    ((uint64_t)identify_struc->lba48_total_sectors[2] << 32) |
	    ((uint64_t)identify_struc->lba48_total_sectors[1] << 16) |
	    ((uint64_t)identify_struc->lba48_total_sectors[0]);

	info->size_bytes = lba48 * 512;
	info->size_kib = info->size_bytes * 1024;
	info->size_mib = info->size_bytes * 1024 * 1024;
	info->size_gib = info->size_bytes * 1024 * 1024 * 1024;
	mem::memcpy(&info->name, &driver_ports[disk_sn].port_name, 16);

	info->media_type = DISKGENERIC_MEDIA_TYPE_AHCI_SATA;

	if (exec_file) {
		uint32_t uuid_on_disk = driver_ports[disk_sn].mbr.unique_disk_id;
		uint32_t limine_boot_disk_id = exec_file->mbr_disk_id;

		if (uuid_on_disk == limine_boot_disk_id) {
			ADPRINTF("Disk %d is the boot disk\n\r", uuid_on_disk);

			info->boot_disk = true;
		} else {
			ADPRINTF("Disk is not boot disk, (on-disk)%d:(limine-resource)%d", uuid_on_disk, limine_boot_disk_id);

			info->boot_disk = false;
		}
	}

	return true;
}
