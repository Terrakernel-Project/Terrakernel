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

struct internal_port {
	HBA_PORT* hba_port;
	uint8_t port_id;
	char port_name[16]; // preferably follow sata_a, sata_b, sata_c, ...
	bool online;
};

internal_port driver_ports[32];
int counter = 0;
const char* port_name_reference_table = "abcdefghijklmnopqrstuvwxyzABCDEF";

void get_next_port_name(char* name) {
	if (!name) return;
	if (counter >= 32) return;
	char port_name[7] = "sata_\0";
	port_name[5] = port_name_reference_table[counter++];
	strncpy(name, port_name, 7);
}

int port_idx = 0;

internal_port* implement_port(HBA_PORT* port) {
	driver_ports[port_idx].hba_port = port;
	driver_ports[port_idx].port_id = port_idx;
	get_next_port_name(driver_ports[port_idx].port_name);
	driver_ports[port_idx].online = false;
	
	return &driver_ports[port_idx++];
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

	ADPRINTF("===============================\n\r\n\r");
}

HBA_MEM* abar;

int64_t ahci_read(uint64_t lba, uint64_t count, uint8_t* buffer, size_t len);
int64_t ahci_write(uint64_t lba, uint64_t count, const uint8_t* data, size_t len);

void ahci_init(pcie_device* dev, disk_driver* driver) {
	printf("Initialising AHCI disk controller\n\r");
	
	driver->name = "AHCI";
	driver->read = ahci_read;
	driver->write = ahci_write;
	
	pcie::enable_bus_mastering(dev);
	
	uint64_t abar_phys = dev->bars[5] & ~0xFULL;
	abar = (HBA_MEM*)mem::vmm::pa_to_va(abar_phys);
	mem::vmm::mmap((void*)abar_phys, abar, (sizeof(HBA_MEM) + 0xFFF) / 0x1000, PAGE_PRESENT | PAGE_RW | PAGE_PCD);
	
	abar->ghc |= (1 << 31);
	
	uint32_t pi = abar->pi;
	
	for (int i = 0; i < 32; i++) {
		if (pi & (1 << i)) {
			internal_port* port = implement_port(&abar->ports[i]);
			setup_port(port);
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

int64_t ahci_read(uint64_t lba, uint64_t count, uint8_t* buffer, size_t len) {
	if (len < (count * 512)) {
		return -1; // buffer size must be bigger than or equal to the number of sectors read
	}

	if (count > 8192) {
		return -1; // cannot read more than 4MiB of data
	}

	ADPRINTF("Reading LBA=%lu count=%lu\n\r", lba, count);
	
	if (port_idx == 0) {
		ADPRINTF("No ports available\n\r");
		return -1;
	}
	
	internal_port* port = &driver_ports[0];
	if (!port->online) {
		ADPRINTF("Port is offline\n\r");
		return -1;
	}
	
	HBA_PORT* hba_port = port->hba_port;
	
	hba_port->is = (uint32_t)-1;
	
	int slot = find_cmdslot(hba_port);
	if (slot == -1) {
		ADPRINTF("No command slot available\n\r");
		return -1;
	}
	ADPRINTF("Using command slot %d\n\r", slot);
	
	uint64_t clb_phys = ((uint64_t)hba_port->clbu << 32) | hba_port->clb;
	HBA_CMD_HEADER* cmd_header = (HBA_CMD_HEADER*)mem::vmm::pa_to_va(clb_phys);
	cmd_header += slot;
	
	cmd_header->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
	cmd_header->w = 0; // Read op
	cmd_header->prdtl = 1;
	cmd_header->prdbc = 0;
	
	uint64_t ctba_phys = ((uint64_t)cmd_header->ctbau << 32) | cmd_header->ctba;
	HBA_CMD_TBL* cmd_tbl = (HBA_CMD_TBL*)mem::vmm::pa_to_va(ctba_phys);
	mem::memset(cmd_tbl, 0, sizeof(HBA_CMD_TBL) + sizeof(HBA_PRDT_ENTRY) * 7);
	
	uint64_t buffer_phys = mem::vmm::va_to_pa((uint64_t)buffer);
	ADPRINTF("Buffer virt=%p phys=0x%lx\n\r", buffer, buffer_phys);
	
	cmd_tbl->prdt_entry[0].dba = (uint32_t)(buffer_phys & 0xFFFFFFFF);
	cmd_tbl->prdt_entry[0].dbau = (uint32_t)(buffer_phys >> 32);
	cmd_tbl->prdt_entry[0].dbc = (count * 512) - 1;
	cmd_tbl->prdt_entry[0].i = 1;
	
	FIS_REG_H2D* fis = (FIS_REG_H2D*)(&cmd_tbl->cfis);
	mem::memset(fis, 0, sizeof(FIS_REG_H2D));
	
	fis->fis_type = FIS_TYPE_REG_H2D;
	fis->c = 1;
	fis->command = 0x25; // READ DMA EXT
	
	fis->lba0 = lba & 0xFF;
	fis->lba1 = (lba >> 8) & 0xFF;
	fis->lba2 = (lba >> 16) & 0xFF;
	fis->device = (1 << 6);
	
	fis->lba3 = (lba >> 24) & 0xFF;
	fis->lba4 = (lba >> 32) & 0xFF;
	fis->lba5 = (lba >> 40) & 0xFF;
	
	fis->countl = count & 0xFF;
	fis->counth = (count >> 8) & 0xFF;
	
	ADPRINTF("Waiting for TFD ready (BSY=0, DRQ=0)\n\r");
	uint64_t timeout_a = 100000;
	while (hba_port->tfd & (0x80 | 0x08) && timeout_a > 0) timeout_a--;
	if (timeout_a <= 0) {
		ADPRINTF("TFD timeout (TFD=0x%08x)\n\r", hba_port->tfd);
		return -1;
	}
	
	ADPRINTF("Issuing command on slot %d\n\r", slot);
	hba_port->ci = 1 << slot;
	
	ADPRINTF("Waiting for completion\n\r");
	uint64_t timeout_b = 10000000;
	while (timeout_b > 0) {
		if ((hba_port->ci & (1 << slot)) == 0) {
			ADPRINTF("Command completed successfully\n\r");
			break;
		}
		
		if (hba_port->is & (1 << 30)) {
			ADPRINTF("Task file error:\n\r");
			ADPRINTF("  IS=0x%08x\n\r", hba_port->is);
			ADPRINTF("  SERR=0x%08x\n\r", hba_port->serr);
			ADPRINTF("  TFD=0x%08x\n\r", hba_port->tfd);
			ADPRINTF("  SSTS=0x%08x\n\r", hba_port->ssts);
			
			hba_port->is = hba_port->is;
			hba_port->serr = hba_port->serr;
			
			return -1;
		}
		
		timeout_b--;
	}
	
	if (timeout_b == 0) {
		ADPRINTF("Command timeout (CI still set)\n\r");
		ADPRINTF("  CI=0x%08x\n\r", hba_port->ci);
		ADPRINTF("  IS=0x%08x\n\r", hba_port->is);
		ADPRINTF("  TFD=0x%08x\n\r", hba_port->tfd);
		return -1;
	}
	
	ADPRINTF("Read completed, transferred %lu bytes\n\r", (uint64_t)cmd_header->prdbc);
	return count * 512;
}

int64_t ahci_write(uint64_t lba, uint64_t count, const uint8_t* data, size_t len) {
	if (len < (count * 512)) {
		return -1; // data size must be bigger than or equal to the number of sectors written
	}

	if (count > 8192) {
		return -1; // cannot write more than 4MiB of data
	}

	ADPRINTF("Writing LBA=%lu count=%lu\n\r", lba, count);
	
	if (port_idx == 0) {
		ADPRINTF("No ports available\n\r");
		return -1;
	}
	
	internal_port* port = &driver_ports[0];
	if (!port->online) {
		ADPRINTF("Port is offline\n\r");
		return -1;
	} else {
		ADPRINTF("Defaulting to %s\n\r", port->port_name);
	}
	
	HBA_PORT* hba_port = port->hba_port;
	
	hba_port->is = (uint32_t)-1;
	
	int slot = find_cmdslot(hba_port);
	if (slot == -1) {
		ADPRINTF("No command slot available\n\r");
		return -1;
	}
	ADPRINTF("Using command slot %d\n\r", slot);
	
	uint64_t clb_phys = ((uint64_t)hba_port->clbu << 32) | hba_port->clb;
	HBA_CMD_HEADER* cmd_header = (HBA_CMD_HEADER*)mem::vmm::pa_to_va(clb_phys);
	cmd_header += slot;
	
	cmd_header->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
	cmd_header->w = 1; // Write op
	cmd_header->prdtl = 1;
	cmd_header->prdbc = 0;
	
	uint64_t ctba_phys = ((uint64_t)cmd_header->ctbau << 32) | cmd_header->ctba;
	HBA_CMD_TBL* cmd_tbl = (HBA_CMD_TBL*)mem::vmm::pa_to_va(ctba_phys);
	mem::memset(cmd_tbl, 0, sizeof(HBA_CMD_TBL) + sizeof(HBA_PRDT_ENTRY) * 7);
	
	uint64_t buffer_phys = mem::vmm::va_to_pa((uint64_t)data);
	ADPRINTF("Buffer virt=%p phys=0x%lx\n\r", data, buffer_phys);
	
	cmd_tbl->prdt_entry[0].dba = (uint32_t)(buffer_phys & 0xFFFFFFFF);
	cmd_tbl->prdt_entry[0].dbau = (uint32_t)(buffer_phys >> 32);
	cmd_tbl->prdt_entry[0].dbc = (count * 512) - 1;
	cmd_tbl->prdt_entry[0].i = 1;
	
	FIS_REG_H2D* fis = (FIS_REG_H2D*)(&cmd_tbl->cfis);
	mem::memset(fis, 0, sizeof(FIS_REG_H2D));
	
	fis->fis_type = FIS_TYPE_REG_H2D;
	fis->c = 1;
	fis->command = 0x35; // WRITE DMA EXT
	
	fis->lba0 = lba & 0xFF;
	fis->lba1 = (lba >> 8) & 0xFF;
	fis->lba2 = (lba >> 16) & 0xFF;
	fis->device = (1 << 6);
	
	fis->lba3 = (lba >> 24) & 0xFF;
	fis->lba4 = (lba >> 32) & 0xFF;
	fis->lba5 = (lba >> 40) & 0xFF;
	
	fis->countl = count & 0xFF;
	fis->counth = (count >> 8) & 0xFF;
	
	ADPRINTF("Waiting for TFD ready\n\r");
	int timeout = 1000000;
	while ((hba_port->tfd & (0x80 | 0x08)) && timeout > 0) {
		timeout--;
	}
	
	if (timeout == 0) {
		ADPRINTF("TFD timeout\n\r");
		return -1;
	}
	
	ADPRINTF("Issuing command\n\r");
	hba_port->ci = 1 << slot;
	
	ADPRINTF("Waiting for completion\n\r");
	timeout = 10000000;
	while (timeout > 0) {
		if ((hba_port->ci & (1 << slot)) == 0) {
			ADPRINTF("Command completed successfully\n\r");
			break;
		}
		if (hba_port->is & (1 << 30)) {
			ADPRINTF("Task file error (IS=0x%08x SERR=0x%08x TFD=0x%08x)\n\r", 
			         hba_port->is, hba_port->serr, hba_port->tfd);
			hba_port->is = hba_port->is;
			hba_port->serr = hba_port->serr;
			return -1;
		}
		timeout--;
	}
	
	if (timeout == 0) {
		ADPRINTF("Command timeout\n\r");
		return -1;
	}
	
	return count * 512;
}
