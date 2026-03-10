#include "nvme.hpp"
#include <config.hpp>
#include <cstdio>

#ifdef CONFIG_NVME_VERBOSE
#	define NDPRINTF(fmt, ...) printf("[ %s ] " fmt, __PRETTY_FUNCTION__, ##__VA_ARGS__);
#else
#	define NDPRINTF(fmt, ...)
#endif

void nvme_init(pcie_device* dev, disk_driver* driver) {
	NDPRINTF("Initialising NVMe driver\n\r");
}
