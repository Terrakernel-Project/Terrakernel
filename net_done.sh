#!/bin/bash

echo "Cleaning up TAP networking..."

# Kill dnsmasq
echo "Stopping dnsmasq..."
sudo killall dnsmasq 2>/dev/null

# Get default network interface
DEFAULT_IF=$(ip route | grep default | awk '{print $5}' | head -n1)

if [ -n "$DEFAULT_IF" ]; then
    echo "Removing NAT rules..."
    
    # Remove NAT rules
    sudo iptables -t nat -D POSTROUTING -s 10.0.0.0/24 -o $DEFAULT_IF -j MASQUERADE 2>/dev/null
    sudo iptables -D FORWARD -i tap0 -o $DEFAULT_IF -j ACCEPT 2>/dev/null
    sudo iptables -D FORWARD -i $DEFAULT_IF -o tap0 -m state --state RELATED,ESTABLISHED -j ACCEPT 2>/dev/null
fi

# Remove tap0 interface
echo "Removing tap0 interface..."
sudo ip link delete tap0 2>/dev/null

echo "Cleanup complete!"
