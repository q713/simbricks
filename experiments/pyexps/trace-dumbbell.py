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
    NodeConfig, I40eLinuxNode, IdleHost, PingClient, I40eDCTCPNode
)
from simbricks.orchestration.simulators import (
    Gem5Host, I40eNIC, SwitchNet, NS3DumbbellNet
)
from simbricks.orchestration.simulator_utils import create_dctcp_hosts
import simbricks.orchestration.experiments as exp

# Start building the actual experiment
e = Experiment(name='trace-dumbbell-experiment')
e.checkpoint = True  # use checkpoint and restore to speed up simulation

# experiment configuration options
# gem5DebugFlags = '--debug-flags=SimBricksAll,SyscallAll,EthernetAll,PciDevice,PciHost,ExecEnable,ExecOpClass,ExecThread,ExecEffAddr,ExecResult,ExecMicro,ExecMacro,ExecUser,ExecKernel,ExecOpClass,ExecRegDelta,ExecFaulting,ExecAsid,ExecFlags,ExecCPSeq,ExecFaulting,ExecFetchSeq'
# named_pipe_folder = "/usr/src/data-folder"
# gem5_server_pipe = f"{named_pipe_folder}/gem5-server-log-pipe.pipe"
# gem5_client_pipe = f"{named_pipe_folder}/gem5-client-log-pipe.pipe"
# nicbm_server_pipe = f"{named_pipe_folder}/nicbm-server-log-pipe.pipe"
# nicbm_client_pipe = f"{named_pipe_folder}/nicbm-client-log-pipe.pipe"
# ns3_pipe = f'{named_pipe_folder}/ns3-log-pipe.pipe'
#ns3_pipe = f' &> /local/jakobg/tracing-experiments/wrkdir/ns3-log-pipe.pipe'
num_pairs = 2
cpu_freq = '5GHz'
mtu = 1500
sys_clock = '1GHz'  # if not set, default 1GHz
ip_start = '192.168.64.1'
eth_latency = 500 * 10**3  # 500 us

# network sim
network = NS3DumbbellNet()
link_rate_gb_s = 10
link_rate_opt = f'--LinkRate={link_rate_gb_s}Gb/s'
link_latency_ns = 500
link_latency_opt = f'--LinkLatency={link_latency_ns}ns'
ecn_th_opt = '--EcnTh=0'
trace_file_path = '/local/jakobg/tracing-experiments/wrkdir/ns3-log-pipe.pipe'
trace_file_opt = f'--EnableTracing={trace_file_path}'
network.opt = f'{link_rate_opt} {link_latency_opt} {ecn_th_opt} {trace_file_path}'
network.eth_latency = eth_latency

e.add_network(network)

# client and server
def gem5_timing(node_config: NodeConfig):
    h = Gem5Host(node_config)
    #h.variant = 'fast'
    #h.sys_clock = sys_clock
    #client.name = 'client'
    #client.wait = True  # wait for client simulator to finish execution
    #gem5_client_log = f'--debug-file {gem5_client_pipe}'
    #client.extra_main_args = [gem5_client_log, gem5DebugFlags]
    #client.variant = 'opt'
    #e.add_host(client)
    #server_config = I40eLinuxNode()  # boot Linux with i40e NIC driver
    #server_config.ip = '10.0.0.2'
    #server_config.app = IdleHost()
    #server = Gem5Host(server_config)
    #server.name = 'server'
    #gem5_server_log = f'--debug-file {gem5_server_pipe}'
    #server.extra_main_args = [gem5_server_log, gem5DebugFlags]
    #server.variant = 'opt'
    return h
client_server_host_class = gem5_timing

def ping_client():
    c = PingClient()
    c.count = 1
    return c
client_app = ping_client

server_app = IdleHost

# nics
def client_server_nic_class_func():
    n = I40eNIC()
    n.eth_latency = eth_latency
    return n
client_server_nic_class = client_server_nic_class_func
client_server_nic_node = I40eLinuxNode #I40eDCTCPNode

# create clients and servers
servers = create_dctcp_hosts(
    e,
    num_pairs,
    'server',
    network,
    client_server_nic_class,
    client_server_host_class,
    client_server_nic_node,
    server_app,
    cpu_freq,
    mtu)

clients = create_dctcp_hosts(
    e,
    num_pairs,
    'client',
    network,
    client_server_nic_class,
    client_server_host_class,
    client_server_nic_node,
    client_app,
    cpu_freq,
    mtu,
    ip_start=num_pairs + 1)

i = 0
for cl in clients:
    cl.node_config.app.server_ip = servers[i].node_config.ip
    i += 1

clients[num_pairs - 1].node_config.app.is_last = True
clients[num_pairs - 1].wait = True

experiments = [e]
