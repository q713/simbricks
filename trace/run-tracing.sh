#
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
#

linux_dump_arg='--linux-dump-server-client ../../linux_dumbs/linux-S.dumb'
echo linux_dump_arg: $linux_dump_arg 

nic_i40e_dump_arg='--nic-i40e-dump ../../linux_dumbs/i40e-driver-S.dumb'
echo nic_i40e_dump_arg $nic_i40e_dump_arg

gem5_server_log_arg='--gem5-log-server /local/jakobg/simbricks-tracing-experiments/simple_netperf/gem5-server-log.log'
nic_server_log_arg='--nicbm-log-server /local/jakobg/simbricks-tracing-experiments/simple_netperf/server-nic.log'
echo gem5_server_log_arg: $gem5_server_log_arg
echo nic_server_log_arg: $nic_server_log_arg


gem5_client_log_arg='--gem5-log-client /local/jakobg/simbricks-tracing-experiments/simple_netperf/gem5-client-log.log'
nic_client_log_arg='--nicbm-log-client /local/jakobg/simbricks-tracing-experiments/simple_netperf/client-nic.log'
echo gem5_client_log_arg: $gem5_client_log_arg
echo nic_client_log_arg: $nic_client_log_arg


min_timestamp_arg='--ts-lower-bound 1578632883625'
echo min_timestamp_arg: $min_timestamp_arg

echo start running the program:
echo ./trace $linux_dump_arg $nic_i40e_dump_arg $gem5_server_log_arg $nic_server_log_arg $gem5_client_log_arg $nic_client_log_arg $min_timestamp_arg
./trace $linux_dump_arg $nic_i40e_dump_arg $gem5_server_log_arg $nic_server_log_arg $gem5_client_log_arg $nic_client_log_arg $min_timestamp_arg