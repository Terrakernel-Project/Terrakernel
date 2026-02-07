#!/bin/bash

echo "Setting up TAP networking for QEMU..."

# Clean up any existing tap0
sudo ip link delete tap0 2>/dev/null

# Create TAP interface
echo "Creating tap0 interface..."
sudo ip tuntap add dev tap0 mode tap user $USER

# Bring it up
echo "Bringing up tap0..."
sudo ip link set tap0 up

# Add IP address
echo "Adding IP 10.0.0.1/24 to tap0..."
sudo ip addr add 10.0.0.1/24 dev tap0

# Enable IP forwarding (for internet access from VM)
echo "Enabling IP forwarding..."
sudo sysctl -w net.ipv4.ip_forward=1 > /dev/null

# Get default network interface
DEFAULT_IF=$(ip route | grep default | awk '{print $5}' | head -n1)

if [ -n "$DEFAULT_IF" ]; then
    echo "Setting up NAT via $DEFAULT_IF..."
    
    # Add NAT rules (for internet access from VM)
    sudo iptables -t nat -A POSTROUTING -s 10.0.0.0/24 -o $DEFAULT_IF -j MASQUERADE
    sudo iptables -A FORWARD -i tap0 -o $DEFAULT_IF -j ACCEPT
    sudo iptables -A FORWARD -i $DEFAULT_IF -o tap0 -m state --state RELATED,ESTABLISHED -j ACCEPT
fi

# Start dnsmasq (DHCP server)
echo "Starting DHCP server on tap0..."
echo "  DHCP range: 10.0.0.10 - 10.0.0.100"
echo "  Gateway: 10.0.0.1"
echo ""
echo "Press Ctrl+C to stop dnsmasq when done"
echo "==============================================="

sudo dnsmasq -d \
  --interface=tap0 \
  --bind-interfaces \
  --dhcp-range=10.0.0.10,10.0.0.100,12h \
  --dhcp-option=3,10.0.0.1 \
  --dhcp-option=6,8.8.8.8 \
  --log-queries \
  --log-dhcp

# Note: Script will keep running until Ctrl+C
# When you press Ctrl+C, run net_done.sh to clean up
