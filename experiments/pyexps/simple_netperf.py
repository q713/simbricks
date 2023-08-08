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
"""
Simple example experiment, which sets up a client and a server host connected
through a switch.

The client 'pings' the server using netperfs TCP_RR.
"""

from simbricks.orchestration.experiments import Experiment
from simbricks.orchestration.nodeconfig import (
    I40eLinuxNode, NetperfServer, SimpleNetperfClient
)
from simbricks.orchestration.simulators import Gem5Host, I40eNIC, SwitchNet

e = Experiment(name='simple_netperf')
e.checkpoint = True  # use checkpoint and restore to speed up simulation

gem5DebugFlags = '--debug-flags=SimBricksAll,SyscallAll,ExecEnable,ExecOpClass,ExecThread,ExecEffAddr,ExecResult,ExecMicro,ExecMacro,ExecFaulting,ExecUser,ExecKernel,EthernetAll,PciDevice,PciHost'

# create client
client_config = I40eLinuxNode()  # boot Linux with i40e NIC driver
client_config.ip = '10.0.0.1'
client_config.app = SimpleNetperfClient(server_ip = '10.0.0.2')
client = Gem5Host(client_config)
client.name = 'client'
client.wait = True  # wait for client simulator to finish execution
gem5_client_log = '--debug-file /OS/endhost-networking/work/sim/jakob/wrkdir/gem5-client-log.log' 
client.extra_main_args = [gem5_client_log, gem5DebugFlags]
client.variant = 'opt'
e.add_host(client)

# attach client's NIC
client_nic = I40eNIC()
client_nic.log_file = '/OS/endhost-networking/work/sim/jakob/wrkdir/client-nic.log'
e.add_nic(client_nic)
client.add_nic(client_nic)

# create server
server_config = I40eLinuxNode()  # boot Linux with i40e NIC driver
server_config.ip = '10.0.0.2'
server_config.app = NetperfServer()
server = Gem5Host(server_config)
server.name = 'server'
gem5_server_log = '--debug-file /OS/endhost-networking/work/sim/jakob/wrkdir/gem5-server-log.log'
server.extra_main_args = [gem5_server_log, gem5DebugFlags]
server.variant = 'opt'
e.add_host(server)

# attach server's NIC
server_nic = I40eNIC()
server_nic.log_file = '/OS/endhost-networking/work/sim/jakob/wrkdir/server-nic.log'
e.add_nic(server_nic)
server.add_nic(server_nic)

# connect NICs over network
network = SwitchNet()
e.add_network(network)
client_nic.set_network(network)
server_nic.set_network(network)

experiments = [e]
