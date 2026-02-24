#ifndef NVME_HPP
#define NVME_HPP 1

#include <pcie/pcie.hpp>
#include <drivers/blockio/diskgeneric.hpp>

void nvme_init(pcie_device* dev, disk_driver* driver);

#endif
