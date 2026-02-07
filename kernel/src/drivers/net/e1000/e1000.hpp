#ifndef E1000_HPP
#define E1000_HPP 1

#include <pcie/pcie.hpp>
#include <drivers/net/netgeneric.hpp>

void e1000_init(pcie_device* dev, net_card_driver* driver);

#endif
