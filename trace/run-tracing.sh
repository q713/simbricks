
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


echo start running the program:
echo ./trace $linux_dump_arg $gem5_server_log_arg $nic_server_log_arg $gem5_client_log_arg $nic_client_log_arg
./trace $linux_dump_arg $gem5_server_log_arg $nic_server_log_arg $gem5_client_log_arg $nic_client_log_arg
