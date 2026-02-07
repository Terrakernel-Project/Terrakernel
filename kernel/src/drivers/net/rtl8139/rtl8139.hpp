#ifndef RTL8139_HPP
#define RTL8139_HPP 1

#include <pcie/pcie.hpp>
#include <drivers/net/netgeneric.hpp>

void rtl8139_init(pcie_device* dev, net_card_driver* driver);

#endif
