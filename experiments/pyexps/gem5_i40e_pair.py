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

import simbricks.orchestration.experiments as exp
import simbricks.orchestration.nodeconfig as node
import simbricks.orchestration.simulators as sim
from simbricks.orchestration.simulator_utils import create_basic_hosts

e = exp.Experiment('gem5-i40e-pair')
#e.timeout = 5 * 60
e.checkpoint = True
net = sim.SwitchNet()
e.add_network(net)

servers = create_basic_hosts(
    e,
    1,
    'server',
    net,
    sim.I40eNIC,
    sim.Gem5Host,
    node.I40eLinuxNode,
    node.IperfTCPServer
)

clients = create_basic_hosts(
    e,
    2,
    'client',
    net,
    sim.I40eNIC,
    sim.Gem5Host,
    node.I40eLinuxNode,
    node.IperfTCPClient,
    ip_start=2
)

gem5DebugFlags = '--debug-flags=SimBricksAll,SyscallAll,ExecAll,EthernetAll,PciDevice,PciHost'
gem5BaseLogFilePath = '--debug-file /OS/endhost-networking/work/sim/jakob/simbricks-fork/experiments/out'

for h in servers + clients:
    h.cpu_type = 'TimingSimpleCPU'
    h.cpu_type_cp = 'TimingSimpleCPU'
    h.variant = 'opt'

for s in servers:
    logFile = gem5BaseLogFilePath + '/gem5-server-log.log'
    s.extra_main_args = [logFile, gem5DebugFlags]
    nic = s.pcidevs[0]
    nic.log_file = '/OS/endhost-networking/work/sim/jakob/simbricks-fork/experiments/out/server-nic.log'

index = 0
for c in clients:
    index += 1
    c.wait = True
    c.node_config.app.server_ip = servers[0].node_config.ip
    logFile = gem5BaseLogFilePath + '/gem5-cliet-{}-log.log'.format(index)
    c.extra_main_args = [logFile, gem5DebugFlags]
    nic = c.pcidevs[0]
    nic.log_file = '/OS/endhost-networking/work/sim/jakob/simbricks-fork/experiments/out/client-{}-nic.log'.format(index)

experiments = [e]
