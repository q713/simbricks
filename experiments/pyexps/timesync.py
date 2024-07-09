# Copyright 2022 Max Planck Institute for Software Systems, and
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

from simbricks.orchestration.experiments import Experiment
from simbricks.orchestration.nodeconfig import (
    I40eLinuxNode, TimesyncNode, ChronyClient, ChronyServer, PTPServer
)
from simbricks.orchestration.simulators import (
    Gem5Host, I40eNIC, NS3DumbbellNet, QemuHost
)
from simbricks.orchestration.simulator_utils import create_dctcp_hosts
import simbricks.orchestration.experiments as exp

#################################
# Start building the actual experiment
#################################
e = Experiment(name='simple-tp-exp')
e.checkpoint = True  # use checkpoint and restore to speed up simulation
servers = []
clients = []
gem5DebugFlags = '--debug-flags=SimBricksAll,SyscallAll,EthernetAll,PciDevice,PciHost,ExecEnable,ExecOpClass,ExecThread,ExecEffAddr,ExecResult,ExecMicro,ExecMacro,ExecUser,ExecKernel,ExecOpClass,ExecRegDelta,ExecFaulting,ExecAsid,ExecFlags,ExecCPSeq,ExecFaulting,ExecFetchSeq'
#named_pipe_folder = "/local/jakobg/tracing-experiments/wrkdir"
named_pipe_folder = "/usr/src/data-folder"
#named_pipe_folder = "/local/jakobg/tracing-experiments/ntp-test-dir"
cpu_freq = '5GHz'
eth_latency_ns = 500
link_latency_ns = 1000
# sync_period_ns = link_latency_ns TODO: link latency and et latency?
sys_clock = '1GHz'  # if not set, default 1GHz
mtu = 1500
num_pairs = 3
use_pressure = True
synchronized = 1
use_ptp = True
chrony_log_level = -1


#################################
# network simulator (ns3)
#################################
network = NS3DumbbellNet('cosim-dumbbell-hybrid-example')
link_rate_gb_s = 1# 10
link_rate_opt = f'--LinkRate={link_rate_gb_s}Gb/s'
link_latency_opt = f'--LinkLatency={link_latency_ns}ns'
ecn_th_opt = '--EcnTh=0'
trace_file_path = f'{named_pipe_folder}/ns3-log-pipe.pipe'
#trace_file_path = f'{named_pipe_folder}/ns3-log-pipe-raw-log.txt'
trace_file_opt = f'--EnableTracing={trace_file_path}'
mtu_opt = f'--Mtu=1500'
ns3_hosts_opt = f'--NumNs3HostPairs={num_pairs}'
#network.opt = f'{link_rate_opt} {link_latency_opt} {ecn_th_opt} {mtu_opt} {ns3_hosts_opt} {trace_file_opt}'
network.opt = f'{link_rate_opt} {link_latency_opt} {ecn_th_opt} {mtu_opt} {ns3_hosts_opt}'
network.eth_latency = eth_latency_ns
network.sync_mode = synchronized

e.add_network(network)

#################################
# server that produces log output
#################################
server_nic = I40eNIC()
server_nic.eth_latency = eth_latency_ns
nicbm_server_pipe = f"{named_pipe_folder}/nicbm-server-log-pipe.pipe"
# server_nic.log_file = nicbm_server_pipe
server_nic.sync_mode = synchronized
server_nic.set_network(network)

server_config = TimesyncNode()
server_config.mtu = mtu
server_config.kcmd_append = 'mce=off'
server_config.ip = '10.0.0.1'
server_config.app = PTPServer() if use_ptp else ChronyServer()

server = Gem5Host(server_config)
server.name = 'server.1'
server.cpu_freq = cpu_freq
server_pipe = f"{named_pipe_folder}/gem5-server-log-pipe.pipe"
server_log = f'--debug-file {server_pipe}'
# server.extra_main_args = [server_log, gem5DebugFlags]
# server.variant = 'opt'
server.sync_mode = synchronized

server.add_nic(server_nic)
e.add_nic(server_nic)
e.add_host(server)

servers.append(server)



#################################
# client that produces log output
#################################
client_nic = I40eNIC()
client_nic.eth_latency = eth_latency_ns
nicbm_client_pipe = f"{named_pipe_folder}/nicbm-client-log-pipe.pipe"
# client_nic.log_file = nicbm_client_pipe
client_nic.sync_mode = synchronized
client_nic.set_network(network)

client_config = TimesyncNode() 
client_config.mtu = mtu
client_config.kcmd_append = 'mce=off'
client_config.ip = '10.0.0.2'
client_config.app = ChronyClient() 
client_config.ntp_server = server_config.ip
client_config.chrony_loglevel = chrony_log_level
client_config.ptp = use_ptp # TODO: enable hw ts 

client = Gem5Host(client_config)
client.name = 'client.1'
client.cpu_freq = cpu_freq
client_pipe = f"{named_pipe_folder}/gem5-client-log-pipe.pipe"
client_log = f'--debug-file {client_pipe}'
# client.extra_main_args = [client_log, gem5DebugFlags]
# client.variant = 'opt'
client.wait = True
client.sync_mode = synchronized

client.add_nic(client_nic)
e.add_nic(client_nic)
e.add_host(client)

clients.append(client)

#################################
# tell client apps about server ips
#################################
assert(len(servers) == len(clients))
assert(len(clients) == 1)


clients[0].node_config.app.server_ip = servers[0].node_config.ip
clients[0].node_config.app.is_last = True
clients[0].wait = True

experiments = [e]
