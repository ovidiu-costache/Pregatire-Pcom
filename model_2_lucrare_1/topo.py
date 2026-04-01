#!/usr/bin/python3

import time
import sys

from mininet.cli import CLI
from mininet.log import setLogLevel
from mininet.net import Mininet
from mininet.topo import Topo
from mininet.util import dumpNodeConnections


N_HOSTS = 4
LOGFILE = "/tmp/debug.txt"

BASE_FORMATS = {
        "host_name": "host{}",
        "router_if_name": "r-{}",
        "host_if_name": "h-{}",
        "router_ip": "192.168.{}.1",
        "host_ip": "192.168.{}.2",
        "router_ip6": "fec0::{:x}:1",
        "router_llip6": "fe80::{:x}:1",
        "host_ip6": "fd00::{:x}:2",
        "host_llip6": "fe80::{:x}:2",
        "router_mac": "DE:FE:C8:ED:00:{:02x}",
        "host_mac": "DE:AD:BE:EF:00:{:02x}",
}


def get(value, host):
    return BASE_FORMATS[value].format(host)

class SingleSwitchTopo(Topo):
    "Single switch connected to n hosts."
    def build(self, n=2):
        switch = self.addHost('router', ip=None)
        # Python's range(N) generates 0..N-1
        for h in range(n):
            host = self.addHost(get("host_name", h), ip=None)
            i1 = get("host_if_name", h)
            i2 = get("router_if_name", h)
            self.addLink(host, switch, intfName1=i1, intfName2=i2)


class NetworkManager(object):
    def __init__(self, net, n_hosts):
        self.net = net
        self.router = self.net.get("router")
        self.hosts = []
        for i in range(n_hosts):
            h = self.net.get(get("host_name", i))
            self.hosts.append(h)

    def setup_ifaces(self):
        for i in range(len(self.hosts)):
            host_ip = get("host_ip", i)
            host_ip6 = get("host_ip6", i)
            host_llip6 = get("host_ip6", i)
            h_if = get("host_if_name", i)

            self.hosts[i].setIP(host_ip, prefixLen=24, intf=h_if)
            self.router.setIP(get("router_ip", i), prefixLen=24,
                              intf=get("router_if_name", i))

    def setup_macs(self):
        for i, host in enumerate(self.hosts):
            h_mac = get("host_mac", i)
            h_if = get("host_if_name", i)
            host.cmd("ifconfig {} hw ether {}".format(h_if, h_mac))

            r_mac = get("router_mac", i)
            r_if = get("router_if_name", i)
            self.router.cmd("ifconfig {} hw ether {}".format(r_if, r_mac))

    def disable_unneeded(self):
        def disable_ipv6_autoconf(host):
            host.cmd("sysctl -w net.ipv6.conf.all.autoconf=0")
            host.cmd("sysctl -w net.ipv6.conf.all.accept_ra=0")

        def disable_ipv6(host):
            host.cmd('sysctl -w net.ipv6.conf.all.disable_ipv6=1')
            host.cmd('sysctl -w net.ipv6.conf.default.disable_ipv6=1')

        def disable_nic_checksum(host, iface):
            host.cmd('ethtool iface {} --offload rx off tx off'.format(iface))
            host.cmd('ethtool -K {} tx-checksum-ip-generic off'.format(iface))

        def disable_arp(host, iface):
            host.cmd("ip link set dev {} arp off".format(iface))

        disable_ipv6_autoconf(self.router)
        disable_ipv6(self.router)

        for i, host in enumerate(self.hosts):
            h_if = get("host_if_name", i)
            r_if = get("router_if_name", i)

            disable_ipv6_autoconf(host)
            disable_ipv6(host)

            disable_nic_checksum(host, h_if)
            disable_nic_checksum(self.router, h_if)

            disable_arp(host, h_if)
            disable_arp(self.router, h_if)

        # we want complete control over these actions
        self.router.cmd('sysctl -w net.ipv4.ip_forward=0')
        self.router.cmd('sysctl -w net.ipv4.icmp_echo_ignore_all=1')

    def add_default_routes(self):
        for i, host in enumerate(self.hosts):
            ip = get("router_ip", i)
            ip6 = get("router_llip6", i)
            h_if = get("host_if_name", i)

            host.cmd("ip -4 route add default via {} dev {}".format(ip, h_if))
            host.cmd("ip -6 route add default via {} dev {}".format(ip6, h_if))

    def add_nei_entries(self):
        for i, host in enumerate(self.hosts):
            ifname = get("host_if_name", i)
            ip = get("router_ip", i)
            ip6 = get("router_llip6", i)
            mac = get("router_mac", i)

            host.cmd("ip -4 neigh add {} lladdr {} dev {}".format(ip, mac, ifname))
            host.cmd("ip -6 neigh add {} lladdr {} dev {}".format(ip6, mac, ifname))

    def setup(self):
        self.disable_unneeded()
        self.setup_ifaces()
        self.setup_macs()
        self.add_nei_entries()
        self.add_default_routes()

def main():
    topo = SingleSwitchTopo(n=N_HOSTS)
    net = Mininet(topo)
    net.start()

    nm = NetworkManager(net, N_HOSTS)
    nm.setup()

    CLI(net)
    net.stop()


topos = { "mytopo": (lambda : SingleSwitchTopo()) }

if __name__ == "__main__":
    # Tell mininet to print useful information
    setLogLevel("info")
    main()
