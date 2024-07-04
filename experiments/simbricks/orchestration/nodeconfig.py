# Copyright 2021 Max Planck Institute for Software Systems, and
# National University of Singapore
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

from __future__ import annotations
from enum import Enum

import io
import tarfile
import typing as tp


class AppConfig():
    """Defines the application to run on a node or host."""

    # pylint: disable=unused-argument
    def run_cmds(self, node: NodeConfig) -> tp.List[str]:
        """Commands to run for this application."""
        return []

    def prepare_pre_cp(self) -> tp.List[str]:
        """Commands to run to prepare this application before checkpointing."""
        return []

    def prepare_post_cp(self) -> tp.List[str]:
        """Commands to run to prepare this application after the checkpoint is
        restored."""
        return []

    def config_files(self) -> tp.Dict[str, tp.IO]:
        """
        Additional files to put inside the node, which are mounted under
        `/tmp/guest/`.

        Specified in the following format: `filename_inside_node`:
        `IO_handle_of_file`
        """
        return {}

    def strfile(self, s: str):
        """
        Helper function to convert a string to an IO handle for usage in
        `config_files()`.

        Using this, you can create a file with the string as its content on the
        node.
        """
        return io.BytesIO(bytes(s, encoding='UTF-8'))


class NodeConfig():
    """Defines the configuration of a node or host."""

    def __init__(self):
        self.sim = 'qemu'
        """The concrete simulator that runs this node config. This is used to
        use execute different commands depending on the concrete simulator,
        e.g., which command to use to end the simulation.

        TODO(Kaufi-Jonas): This is ugly. Refactor necessary commands to be
        provided by the simulator's class directly."""
        self.ip = '10.0.0.1'
        """IP address."""
        self.prefix = 24
        """IP prefix."""
        self.cores = 1
        """Number of CPU cores."""
        self.threads = 1
        """Number of threads per CPU core."""
        self.memory = 8 * 1024
        """Amount of system memory in MB."""
        self.disk_image = 'base'
        """Name of disk image to use."""
        self.mtu = 1500
        """Networking MTU."""
        self.nockp = 0
        """Do not create a checkpoint in Gem5.

        TODO(Kaufi-Jonas): Seems we don't need this anymore since we specify
        whether to take a checkpoint experiment-wide. Otherwise, refactor this
        into simulator-specific class.
        """
        self.app: tp.Optional[AppConfig] = None
        """Application to run on simulated host."""
        self.kcmd_append = ''
        """String to be appended to kernel command line."""

    def config_str(self) -> str:
        if self.sim == 'gem5':
            cp_es = [] if self.nockp else ['m5 checkpoint']
            exit_es = ['m5 exit']
        else:
            cp_es = ['echo ready to checkpoint']
            exit_es = ['poweroff -f']

        es = self.prepare_pre_cp() + self.app.prepare_pre_cp() + cp_es + \
             self.prepare_post_cp() + self.app.prepare_post_cp() + \
             self.run_cmds() + self.cleanup_cmds() + exit_es
        return '\n'.join(es)

    def make_tar(self, path):
        with tarfile.open(path, 'w:') as tar:
            # add main run script
            cfg_i = tarfile.TarInfo('guest/run.sh')
            cfg_i.mode = 0o777
            cfg_f = self.strfile(self.config_str())
            cfg_f.seek(0, io.SEEK_END)
            cfg_i.size = cfg_f.tell()
            cfg_f.seek(0, io.SEEK_SET)
            tar.addfile(tarinfo=cfg_i, fileobj=cfg_f)
            cfg_f.close()

            # add additional config files
            for (n, f) in self.config_files().items():
                f_i = tarfile.TarInfo('guest/' + n)
                f_i.mode = 0o777
                f.seek(0, io.SEEK_END)
                f_i.size = f.tell()
                f.seek(0, io.SEEK_SET)
                tar.addfile(tarinfo=f_i, fileobj=f)
                f.close()

    def prepare_pre_cp(self) -> tp.List[str]:
        """Commands to run to prepare node before checkpointing."""
        return [
            'set -x',
            'export HOME=/root',
            'export LANG=en_US',
            'export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:' + \
            '/usr/bin:/sbin:/bin:/usr/games:/usr/local/games"'
        ]

    def prepare_post_cp(self) -> tp.List[str]:
        """Commands to run to prepare node after checkpoint restore."""
        return []

    def run_cmds(self) -> tp.List[str]:
        """Commands to run on node."""
        return self.app.run_cmds(self)

    def cleanup_cmds(self) -> tp.List[str]:
        """Commands to run to cleanup node."""
        return []

    def config_files(self) -> tp.Dict[str, tp.IO]:
        """
        Additional files to put inside the node, which are mounted under
        `/tmp/guest/`.

        Specified in the following format: `filename_inside_node`:
        `IO_handle_of_file`
        """
        return self.app.config_files()

    def strfile(self, s: str):
        """
        Helper function to convert a string to an IO handle for usage in
        `config_files()`.

        Using this, you can create a file with the string as its content on the
        node.
        """
        return io.BytesIO(bytes(s, encoding='UTF-8'))


class LinuxNode(NodeConfig):

    def __init__(self):
        super().__init__()
        self.ifname = 'eth0'
        self.drivers = []
        self.force_mac_addr = None

    def prepare_post_cp(self):
        l = []
        for d in self.drivers:
            if d[0] == '/':
                l.append('insmod ' + d)
            else:
                l.append('modprobe ' + d)
        if self.force_mac_addr:
            l.append(
                'ip link set dev ' + self.ifname + ' address ' +
                self.force_mac_addr
            )
        l.append('ip link set dev ' + self.ifname + ' up')
        l.append(f'ip addr add {self.ip}/{self.prefix} dev {self.ifname}')
        return super().prepare_post_cp() + l


class I40eLinuxNode(LinuxNode):

    def __init__(self):
        super().__init__()
        self.drivers.append('i40e')

class TimesyncNode(I40eLinuxNode):

    def __init__(self):
        super().__init__()
        self.disk_image = 'timesync'
        self.memory = 8192

    def prepare_pre_cp(self):
        return super().prepare_pre_cp() + [
            'mount -t proc proc /proc',
            'mount -t sysfs sysfs /sys',
            'ip link set dev lo up',
            #'ip addr add 127.0.0.1/8 dev lo',
        ]


class CorundumLinuxNode(LinuxNode):

    def __init__(self):
        super().__init__()
        self.drivers.append('/tmp/guest/mqnic.ko')

    # pylint: disable=consider-using-with
    def config_files(self):
        m = {'mqnic.ko': open('../images/mqnic/mqnic.ko', 'rb')}
        return {**m, **super().config_files()}


class E1000LinuxNode(LinuxNode):

    def __init__(self):
        super().__init__()
        self.drivers.append('e1000')


class MtcpNode(NodeConfig):

    def __init__(self):
        super().__init__()
        self.disk_image = 'mtcp'
        self.pci_dev = '0000:00:02.0'
        self.memory = 16 * 1024
        self.num_hugepages = 4096

    def prepare_pre_cp(self):
        return super().prepare_pre_cp() + [
            'mount -t proc proc /proc',
            'mount -t sysfs sysfs /sys',
            'mkdir -p /dev/hugepages',
            'mount -t hugetlbfs nodev /dev/hugepages',
            'mkdir -p /dev/shm',
            'mount -t tmpfs tmpfs /dev/shm',
            'echo ' + str(self.num_hugepages) + ' > /sys/devices/system/' + \
            'node/node0/hugepages/hugepages-2048kB/nr_hugepages',
            ]

    def prepare_post_cp(self):
        return super().prepare_post_cp() + [
            'insmod /root/mtcp/dpdk/x86_64-native-linuxapp-gcc/kmod/igb_uio.ko',
            '/root/mtcp/dpdk/usertools/dpdk-devbind.py -b igb_uio ' +
            self.pci_dev,
            'insmod /root/mtcp/dpdk-iface-kmod/dpdk_iface.ko',
            '/root/mtcp/dpdk-iface-kmod/dpdk_iface_main',
            'ip link set dev dpdk0 up',
            f'ip addr add {self.ip}/{self.prefix} dev dpdk0'
        ]

    def config_files(self):
        m = {
            'mtcp.conf':
                self.strfile(
                    'io = dpdk\n'
                    'num_cores = ' + str(self.cores) + '\n'
                                                       'num_mem_ch = 4\n'
                                                       'port = dpdk0\n'
                                                       'max_concurrency = 4096\n'
                                                       'max_num_buffers = 4096\n'
                                                       'rcvbuf = 8192\n'
                                                       'sndbuf = 8192\n'
                                                       'tcp_timeout = 10\n'
                                                       'tcp_timewait = 0\n'
                                                       '#stat_print = dpdk0\n'
                )
        }

        return {**m, **super().config_files()}


class TASNode(NodeConfig):

    def __init__(self):
        super().__init__()
        self.disk_image = 'tas'
        self.pci_dev = '0000:00:02.0'
        self.memory = 16 * 1024
        self.num_hugepages = 4096
        self.fp_cores = 1
        self.preload = True

    def prepare_pre_cp(self):
        return super().prepare_pre_cp() + [
            'mount -t proc proc /proc',
            'mount -t sysfs sysfs /sys',
            'mkdir -p /dev/hugepages',
            'mount -t hugetlbfs nodev /dev/hugepages',
            'mkdir -p /dev/shm',
            'mount -t tmpfs tmpfs /dev/shm',
            'echo ' + str(self.num_hugepages) + ' > /sys/devices/system/' + \
            'node/node0/hugepages/hugepages-2048kB/nr_hugepages',
            ]

    def prepare_post_cp(self):
        cmds = super().prepare_post_cp() + [
            'insmod /root/dpdk/lib/modules/5.4.46/extra/dpdk/igb_uio.ko',
            '/root/dpdk/sbin/dpdk-devbind -b igb_uio ' + self.pci_dev,
            'cd /root/tas',
            (
                f'tas/tas --ip-addr={self.ip}/{self.prefix}'
                f' --fp-cores-max={self.fp_cores} --fp-no-ints &'
            ),
            'sleep 1'
        ]

        if self.preload:
            cmds += ['export LD_PRELOAD=/root/tas/lib/libtas_interpose.so']
        return cmds


class I40eDCTCPNode(NodeConfig):

    def prepare_pre_cp(self):
        return super().prepare_pre_cp() + [
            'mount -t proc proc /proc',
            'mount -t sysfs sysfs /sys',
            'sysctl -w net.core.rmem_default=31457280',
            'sysctl -w net.core.rmem_max=31457280',
            'sysctl -w net.core.wmem_default=31457280',
            'sysctl -w net.core.wmem_max=31457280',
            'sysctl -w net.core.optmem_max=25165824',
            'sysctl -w net.ipv4.tcp_mem="786432 1048576 26777216"',
            'sysctl -w net.ipv4.tcp_rmem="8192 87380 33554432"',
            'sysctl -w net.ipv4.tcp_wmem="8192 87380 33554432"',
            'sysctl -w net.ipv4.tcp_congestion_control=dctcp',
            'sysctl -w net.ipv4.tcp_ecn=1'
        ]

    def prepare_post_cp(self):
        return super().prepare_post_cp() + [
            'modprobe i40e',
            'ethtool -G eth0 rx 4096 tx 4096',
            'ethtool -K eth0 tso off',
            'ip link set eth0 txqueuelen 13888',
            f'ip link set dev eth0 mtu {self.mtu} up',
            f'ip addr add {self.ip}/{self.prefix} dev eth0',
        ]


class CorundumDCTCPNode(NodeConfig):

    def prepare_pre_cp(self):
        return super().prepare_pre_cp() + [
            'mount -t proc proc /proc',
            'mount -t sysfs sysfs /sys',
            'sysctl -w net.core.rmem_default=31457280',
            'sysctl -w net.core.rmem_max=31457280',
            'sysctl -w net.core.wmem_default=31457280',
            'sysctl -w net.core.wmem_max=31457280',
            'sysctl -w net.core.optmem_max=25165824',
            'sysctl -w net.ipv4.tcp_mem="786432 1048576 26777216"',
            'sysctl -w net.ipv4.tcp_rmem="8192 87380 33554432"',
            'sysctl -w net.ipv4.tcp_wmem="8192 87380 33554432"',
            'sysctl -w net.ipv4.tcp_congestion_control=dctcp',
            'sysctl -w net.ipv4.tcp_ecn=1'
        ]

    def prepare_post_cp(self):
        return super().prepare_post_cp() + [
            'insmod mqnic.ko',
            'ip link set dev eth0 up',
            f'ip addr add {self.ip}/{self.prefix} dev eth0',
        ]


class LinuxFEMUNode(NodeConfig):

    def __init__(self):
        super().__init__()
        self.drivers = ['nvme']

    def prepare_post_cp(self):
        l = ['lspci -vvvv']
        for d in self.drivers:
            if d[0] == '/':
                l.append('insmod ' + d)
            else:
                l.append('modprobe ' + d)
        return super().prepare_post_cp() + l


class IdleHost(AppConfig):

    def run_cmds(self, node):
        return ['sleep infinity']


class NVMEFsTest(AppConfig):

    def run_cmds(self, node):
        return [
            'mount -t proc proc /proc',
            'mkfs.ext3 /dev/nvme0n1',
            'mount /dev/nvme0n1 /mnt',
            'dd if=/dev/urandom of=/mnt/foo bs=1024 count=1024'
        ]


class DctcpServer(AppConfig):

    def run_cmds(self, node):
        return ['iperf -s -w 1M -Z dctcp']


class DctcpClient(AppConfig):

    def __init__(self):
        super().__init__()
        self.server_ip = '192.168.64.1'
        self.is_last = False

    def run_cmds(self, node):
        if self.is_last:
            return [
                'sleep 1',
                f'iperf -w 1M -c {self.server_ip} -Z dctcp -i 1',
                'sleep 2'
            ]
        else:
            return [
                'sleep 1',
                f'iperf -w 1M -c {self.server_ip} -Z dctcp -i 1',
                'sleep 20'
            ]


class PingClient(AppConfig):

    def __init__(self, server_ip='192.168.64.1', count=100):
        super().__init__()
        self.server_ip = server_ip
        self.count = count

    def run_cmds(self, node):
        return [f'ping {self.server_ip} -c {self.count}']


class IperfTCPServer(AppConfig):

    def run_cmds(self, node):
        return ['iperf -s -l 32M -w 32M']


class IperfUDPServer(AppConfig):

    def run_cmds(self, node):
        return ['iperf -s -u']


class IperfTCPClient(AppConfig):

    def __init__(self):
        super().__init__()
        self.server_ip = '10.0.0.1'
        self.procs = 1
        self.is_last = False

    def run_cmds(self, node):

        cmds = [
            'sleep 1',
            'iperf -l 32M -w 32M  -c ' + self.server_ip + ' -i 1 -P ' +
            str(self.procs)
        ]
        if self.is_last:
            cmds.append('sleep 0.5')
        else:
            cmds.append('sleep 10')
        return cmds


class IperfUDPClient(AppConfig):

    def __init__(self):
        super().__init__()
        self.server_ip = '10.0.0.1'
        self.rate = '150m'
        self.is_last = False

    def run_cmds(self, node):
        cmds = [
            'sleep 1',
            'iperf -c ' + self.server_ip + ' -i 1 -u -b ' + self.rate
        ]

        if self.is_last:
            cmds.append('sleep 0.5')
        else:
            cmds.append('sleep 10')

        return cmds


class IperfUDPShortClient(AppConfig):

    def __init__(self):
        super().__init__()
        self.server_ip = '10.0.0.1'
        self.rate = '150m'
        self.is_last = False

    def run_cmds(self, node):
        cmds = ['sleep 1', 'iperf -c ' + self.server_ip + ' -u -n 1 ']

        return cmds


class IperfUDPClientSleep(AppConfig):

    def __init__(self):
        super().__init__()
        self.server_ip = '10.0.0.1'
        self.rate = '150m'

    def run_cmds(self, node):
        return ['sleep 1', 'sleep 10']


class NoTraffic(AppConfig):

    def __init__(self):
        super().__init__()
        self.is_sleep = 1
        self.is_server = 0

    def run_cmds(self, node):
        cmds = []
        if self.is_server:
            cmds.append('sleep infinity')
        else:
            if self.is_sleep:
                cmds.append('sleep 10')

            else:
                cmds.append('dd if=/dev/urandom of=/dev/null count=500000')

        return cmds


class NetperfServer(AppConfig):

    def run_cmds(self, node):
        return ['netserver', 'sleep infinity']


class NetperfClient(AppConfig):

    def __init__(self):
        super().__init__()
        self.server_ip = '10.0.0.1'
        self.duration_tp = 10
        self.duration_lat = 10

    def run_cmds(self, node):
        return [
            'netserver',
            'sleep 0.5',
            f'netperf -H {self.server_ip} -l {self.duration_tp}',
            (
                f'netperf -H {self.server_ip} -l {self.duration_lat} -t TCP_RR'
                ' -- -o mean_latency,p50_latency,p90_latency,p99_latency'
            )
        ]

class ColumboNetperfClient(AppConfig):
    class SenderType(Enum):
        # Request Response tests a.k.a latency measurement
        TCP_RR = 0
        # Bulk transfer tests a.k.a throughput
        TCP_STREAM = 1

        def __str__(self):
            return self.name

        @classmethod
        def to_string(cls) -> str:
            # cls here is the enumeration
            return str(cls)

    def __init__(self, server_ip='10.0.0.1', sender_type=SenderType.TCP_RR, test_len=10):
        super().__init__()
        self.server_ip = server_ip
        self.sender_type = sender_type
        self.test_len = test_len

    def run_cmds(self, node):
        return [
            f'netperf -H {self.server_ip} -l {self.test_len} -t {self.sender_type}'
            ' -- -o mean_latency,p50_latency,p90_latency,p99_latency',
            'sleep 1'
        ]

class NTPServer(AppConfig):

    def __init__(self):
        super().__init__()
        # see install-bash.sh

    def prepare_pre_cp(self):
        return super().prepare_pre_cp() + [
            f"""
echo "driftfile /var/lib/chrony/drift" >> /etc/chrochronyc trackingny.conf
echo "local stratum 1" >> /etc/chrony.conf
echo "allow 192.168.64.0/24" >> /etc/chrony.conf
echo "ratelimit interval -2" >> /etc/chrony.conf
echo "for i in {{0..60}}" >> query.sh
echo "do" >> query.sh
echo "    date +%s%N" >> query.sh
echo "    m5 dumpstats" >> query.sh
echo "    chronyc -n tracking" >> query.sh
echo "    sleep 60" >> query.sh
echo "done" >> query.sh
chmod +x query.sh
"""
        ]

    def run_cmds(self, node):
        return [
            """
            chronyd -4 -d -d -f /etc/chrony.conf &
            ./query.sh &
            sleep infinity
            """
        ]

class NTPClient(AppConfig):

    def __init__(self, server_ip='10.0.0.1'):
        super().__init__()
        # see install-bash.sh
        self.server_ip = server_ip

    def prepare_pre_cp(self):
        # minpoll 2 maxpoll 2
        # echo "makestep 0.1 3" >> /etc/chrony.conf
        # echo "makestep 1 -1" >> /etc/chrony.conf
        # echo "    chronyc -n sourcestats" >> query.sh
        # echo "    chronyc -n sources" >> query.sh
        return super().prepare_pre_cp() + [
            f"""
echo "driftfile /var/lib/chrony/drift" >> /etc/chrony.conf
echo "server {self.server_ip} iburst " >> /etc/chrony.conf

echo "for i in {{0..60}}" >> query.sh
echo "do" >> query.sh
echo "    date +%s%N" >> query.sh
echo "    m5 dumpstats" >> query.sh
echo "    chronyc -n tracking" >> query.sh
echo "    sleep 60" >> query.sh
echo "done" >> query.sh
chmod +x query.sh
"""
        ]

    def run_cmds(self, node):
        return [
            """
            chronyd -4 -d -d -f /etc/chrony.conf &
            ./query.sh &
            pid=$!
            wait $pid
            """
        ]

class PTPServer(AppConfig):

    def __init__(self):
        super().__init__()

    def prepare_pre_cp(self):
        cmds = super().prepare_pre_cp()
        # lower clock class -> higher priority to ensure this serve
        # ends up grand master
        cmds.append('sed -i '
                    '-e "s/clockClass\t*[0-9-]*/clockClass\t128/g" '
                    '-e "s/logAnnounceInterval\t*[0-9-]*/logAnnounceInterval\t-2/g" '
                    '-e "s/logSyncInterval\t*[0-9-]*/logSyncInterval\t-5/g" '
                    '-e "s/logMinDelayReqInterval\t*[0-9-]*/logMinDelayReqInterval\t-5/g" '
                    '-e "s/logMinPdelayReqInterval\t*[0-9-]*/logMinPdelayReqInterval\t-5/g" '
                    '-e "s/operLogSyncInterval[\t ]*[0-9-]*/operLogSyncInterval\t-5/g" '
                    '-e "s/operLogPdelayReqInterval[\t ]*[0-9-]*/operLogPdelayReqInterval\t-5/g" '
                    '/etc/linuxptp/ptp4l.conf')
        cmds.append('cat /etc/linuxptp/ptp4l.conf')

        # m5 and sys time query
        cmds = cmds + [f"""
echo "for i in {{0..60}}" >> sys-query.sh
echo "do" >> sys-query.sh
echo "    date +%s%N" >> sys-query.sh
echo "    m5 dumpstats" >> sys-query.sh
echo "    sleep 60" >> sys-query.sh
echo "done" >> sys-query.sh
chmod +x sys-query.sh
"""]
        return cmds

    def config_files(self):
        cfg = (
            f'bindcmdaddress 127.0.0.1\n'
            f'allow 10.0.0.0/8\n'
            f'driftfile /tmp/chrony-drift\n'
            f'local stratum 1\n'
            )
        m = {'chrony.conf': self.strfile(cfg)}
        return m

    def run_cmds(self, node):
        return [
            # initially set phc to system time, so we have a sane starting
            # point
            f'phc_ctl /dev/ptp0 set &',
            f'ptp4l -m -q -f /etc/linuxptp/ptp4l.conf -i eth0 &',
            f"""
./sys-query.sh &
pid=$!
wait $pid
"""
        ]


class ChronyServer(AppConfig):

    def __init__(self):
        super().__init__()
        self.loglevel = 0
        self.nic_timestamping = False

    def prepare_pre_cp(self):
        cmds = super().prepare_pre_cp()
        
        # m5 and sys time query
        cmds = cmds + [f"""
echo "for i in {{0..60}}" >> sys-query.sh
echo "do" >> sys-query.sh
echo "  date +%s%N" >> sys-query.sh
echo "  m5 dumpstats" >> sys-query.sh
echo "  sleep 60" >> sys-query.sh
echo "done" >> sys-query.sh
chmod +x sys-query.sh
"""     
        ]
        # chrony query
        cmds = cmds + [f"""
echo "for i in {{0..60}}" >> chrony-query.sh
echo "do" >> chrony-query.sh
echo "  chronyc -n tracking" >> chrony-query.sh
echo "  sleep 60" >> chrony-query.sh
echo "done" >> chrony-query.sh
chmod +x chrony-query.sh
"""     
        ]

        return cmds

    def config_files(self):
        cfg = (
            f'bindcmdaddress 127.0.0.1\n'
            f'allow 10.0.0.0/8\n'
            f'driftfile /tmp/chrony-drift\n'
            f'local stratum 1\n'
            )
        if self.nic_timestamping:
            cfg += 'hwtimestamp * rxfilter ptp\n'
            cfg += 'ptpport 319\n'
        m = {'chrony.conf': self.strfile(cfg)}
        return m

    def run_cmds(self, node):
        return [f'chronyd -d -d -x -f chrony.conf -L {self.loglevel} &', 
                f"""
./chrony-query.sh &
./sys-query.sh &
pid=$!
wait $pid
"""
        ]


class ChronyClient(AppConfig):

    def __init__(self):
        super().__init__()
        self.chrony_loglevel = 0
        self.ntp_server = '10.0.0.1'
        self.nic_timestamping = False
        self.ptp = False

    def prepare_pre_cp(self):
        cmds = super().prepare_pre_cp()
        if self.ptp:
            cmds.append('sed -i '
                        '-e "s/logAnnounceInterval\t*[0-9-]*/logAnnounceInterval\t-2/g" '
                        '-e "s/logSyncInterval\t*[0-9-]*/logSyncInterval\t-5/g" '
                        '-e "s/logMinDelayReqInterval\t*[0-9-]*/logMinDelayReqInterval\t-5/g" '
                        '-e "s/logMinPdelayReqInterval\t*[0-9-]*/logMinPdelayReqInterval\t-5/g" '
                        '-e "s/operLogSyncInterval[\t ]*[0-9-]*/operLogSyncInterval\t-5/g" '
                        '-e "s/operLogPdelayReqInterval[\t ]*[0-9-]*/operLogPdelayReqInterval\t-5/g" '
                        '/etc/linuxptp/ptp4l.conf')
            cmds.append('cat /etc/linuxptp/ptp4l.conf')

        # m5 and sys time query
        cmds = cmds + [f"""
echo "for i in {{0..60}}" >> sys-query.sh
echo "do" >> sys-query.sh
echo "  date +%s%N" >> sys-query.sh
echo "  m5 dumpstats" >> sys-query.sh
echo "  sleep 60" >> sys-query.sh
echo "done" >> sys-query.sh
chmod +x sys-query.sh
"""
        ]
        # chrony query
        cmds = cmds + [f"""
echo "for i in {{0..60}}" >> chrony-query.sh
echo "do" >> chrony-query.sh
echo "  chronyc -n tracking" >> chrony-query.sh
echo "  sleep 60" >> chrony-query.sh
echo "done" >> chrony-query.sh
chmod +x chrony-query.sh
"""
        ]

        return cmds


    def config_files(self):
        if self.ptp:
            cfg = (
                f'bindcmdaddress 127.0.0.1\n'
                f'refclock PHC /dev/ptp0 poll -2 dpoll -3\n'
                f'driftfile /tmp/chrony-drift\n'
                f'makestep 0.01 3\n'
                )
        else:
            ptpport = ''
            if self.nic_timestamping:
                ptpport = 'port 319'
            cfg = (
                f'bindcmdaddress 127.0.0.1\n'
                f'server {self.ntp_server} iburst minpoll -6 maxpoll -1 xleave {ptpport}\n'
                f'driftfile /tmp/chrony-drift\n'
                f'makestep 0.01 3\n'
                )
            if self.nic_timestamping:
                cfg += 'hwtimestamp * rxfilter ptp\n'
                cfg += 'ptpport 319\n'
        m = {'chrony.conf': self.strfile(cfg)}
        return m

    def run_cmds(self, node):
        #cmds = [f'sleep 0.5',
        #        f'chronyd -d -f chrony.conf -L {self.chrony_loglevel} &',
        #        f'sleep 1',
        #        f'(while true; do chronyc tracking; sleep 1; done) &']
        cmds = [f'chronyd -d -d -f chrony.conf -L {self.chrony_loglevel} &']
        if self.ptp:
            cmds = [f'ptp4l -m -q -f /etc/linuxptp/ptp4l.conf -i eth0 &'] + cmds

        cmds = cmds + [f"""
./chrony-query.sh &
./sys-query.sh &
pid=$!
wait $pid
"""
        ]

        return cmds


class VRReplica(AppConfig):

    def __init__(self):
        super().__init__()
        self.index = 0

    def run_cmds(self, node):
        return [
            '/root/nopaxos/bench/replica -c /root/nopaxos.config -i ' +
            str(self.index) + ' -m vr'
        ]


class VRClient(AppConfig):

    def __init__(self):
        super().__init__()
        self.server_ips = []

    def run_cmds(self, node):
        cmds = []
        for ip in self.server_ips:
            cmds.append('ping -c 2 ' + ip)
        cmds.append(
            '/root/nopaxos/bench/client -c /root/nopaxos.config ' +
            '-m vr -u 2 -h ' + node.ip
        )
        return cmds


class NOPaxosReplica(AppConfig):

    def __init__(self):
        super().__init__()
        self.index = 0

    def run_cmds(self, node):
        return [
            '/root/nopaxos/bench/replica -c /root/nopaxos.config -i ' +
            str(self.index) + ' -m nopaxos'
        ]


class NOPaxosClient(AppConfig):

    def __init__(self):
        super().__init__()
        self.server_ips = []
        self.is_last = False
        self.use_ehseq = False

    def run_cmds(self, node):
        cmds = []
        for ip in self.server_ips:
            cmds.append('ping -c 2 ' + ip)
        cmd = '/root/nopaxos/bench/client -c /root/nopaxos.config ' + \
              '-m nopaxos -u 2 -h ' + node.ip
        if self.use_ehseq:
            cmd += ' -e'
        cmds.append(cmd)
        if self.is_last:
            cmds.append('sleep 1')
        else:
            cmds.append('sleep infinity')
        return cmds


class NOPaxosSequencer(AppConfig):

    def run_cmds(self, node):
        return [(
            '/root/nopaxos/sequencer/sequencer -c /root/nopaxos.config'
            ' -m nopaxos'
        )]


class RPCServer(AppConfig):

    def __init__(self):
        super().__init__()
        self.port = 1234
        self.threads = 1
        self.max_flows = 1234
        self.max_bytes = 1024

    def run_cmds(self, node):
        exe = 'echoserver_linux' if not isinstance(node, MtcpNode) else \
            'echoserver_mtcp'
        return [
            'cd /root/tasbench/micro_rpc',
            (
                f'./{exe} {self.port} {self.threads} /tmp/guest/mtcp.conf'
                f' {self.max_flows} {self.max_bytes}'
            )
        ]


class RPCClient(AppConfig):

    def __init__(self):
        super().__init__()
        self.server_ip = '10.0.0.1'
        self.port = 1234
        self.threads = 1
        self.max_flows = 128
        self.max_bytes = 1024
        self.max_pending = 1
        self.openall_delay = 2
        self.max_msgs_conn = 0
        self.max_pend_conns = 8
        self.time = 25

    def run_cmds(self, node):
        exe = 'testclient_linux' if not isinstance(node, MtcpNode) else \
            'testclient_mtcp'
        return [
            'cd /root/tasbench/micro_rpc',
            (
                f'./{exe} {self.server_ip} {self.port} {self.threads}'
                f' /tmp/guest/mtcp.conf {self.max_bytes} {self.max_pending}'
                f' {self.max_flows} {self.openall_delay} {self.max_msgs_conn}'
                f' {self.max_pend_conns} &'
            ),
            f'sleep {self.time}'
        ]


################################################################################


class HTTPD(AppConfig):

    def __init__(self):
        super().__init__()
        self.threads = 1
        self.file_size = 64
        self.mtcp_config = 'lighttpd.conf'
        self.httpd_dir = ''  # TODO added because doesn't originally exist

    def prepare_pre_cp(self):
        return [
            'mkdir -p /srv/www/htdocs/ /tmp/lighttpd/',
            (
                f'dd if=/dev/zero of=/srv/www/htdocs/file bs={self.file_size}'
                ' count=1'
            )
        ]

    def run_cmds(self, node):
        return [
            f'cd {self.httpd_dir}/src/',
            (
                f'./lighttpd -D -f ../doc/config/{self.mtcp_config}'
                f' -n {self.threads} -m ./.libs/'
            )
        ]


class HTTPDLinux(HTTPD):

    def __init__(self):
        super().__init__()
        self.httpd_dir = '/root/mtcp/apps/lighttpd-mtlinux'


class HTTPDLinuxRPO(HTTPD):

    def __init__(self):
        super().__init__()
        self.httpd_dir = '/root/mtcp/apps/lighttpd-mtlinux-rop'


class HTTPDMtcp(HTTPD):

    def __init__(self):
        super().__init__()
        self.httpd_dir = '/root/mtcp/apps/lighttpd-mtcp'
        self.mtcp_config = 'm-lighttpd.conf'

    def prepare_pre_cp(self):
        return super().prepare_pre_cp() + [
            f'cp /tmp/guest/mtcp.conf {self.httpd_dir}/src/mtcp.conf',
            (
                'sed -i "s:^server.document-root =.*:server.document-root = '
                'server_root + \\"/htdocs\\":" '
                f'{self.httpd_dir}/doc/config/{self.mtcp_config}'
            )
        ]


class HTTPC(AppConfig):

    def __init__(self):
        super().__init__()
        self.server_ip = '10.0.0.1'
        self.conns = 1000
        # self.requests = 10000000
        self.requests = 10000
        self.threads = 1
        self.url = '/file'
        self.ab_dir = ''  # TODO added because doesn't originally exist

    def run_cmds(self, node):
        return [
            f'cd {self.ab_dir}/support/',
            (
                f'./ab -N {self.threads} -c {self.conns} -n {self.requests}'
                f' {self.server_ip}{self.url}'
            )
        ]


class HTTPCLinux(HTTPC):

    def __init__(self):
        super().__init__()
        self.ab_dir = '/root/mtcp/apps/ab-linux'


class HTTPCMtcp(HTTPC):

    def __init__(self):
        super().__init__()
        self.ab_dir = '/root/mtcp/apps/ab-mtcp'

    def prepare_pre_cp(self):
        return super().prepare_pre_cp() + [
            f'cp /tmp/guest/mtcp.conf {self.ab_dir}/support/config/mtcp.conf',
            f'rm -f {self.ab_dir}/support/config/arp.conf'
        ]


class MemcachedServer(AppConfig):

    def run_cmds(self, node):
        return ['memcached -u root -t 1 -c 4096']


class MemcachedClient(AppConfig):

    def __init__(self):
        super().__init__()
        self.server_ips = ['10.0.0.1']
        self.threads = 1
        self.concurrency = 1
        self.throughput = '1k'

    def run_cmds(self, node):
        servers = [ip + ':11211' for ip in self.server_ips]
        servers = ','.join(servers)
        return [(
            f'memaslap --binary --time 10s --server={servers}'
            f' --thread={self.threads} --concurrency={self.concurrency}'
            f' --tps={self.throughput} --verbose'
        )]
