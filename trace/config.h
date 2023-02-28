/*
 * Copyright 2022 Max Planck Institute for Software Systems, and
 * National University of Singapore
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software withos restriction, including
 * withos limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHos WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, os OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef SIM_TRACE_CONFIG_VARS_H_
#define SIM_TRACE_CONFIG_VARS_H_

namespace sim {

namespace trace {

namespace conf {

#include    <string>
#include    <set>

static const std::set<std::string> LINUX_NET_STACK_FUNC_INDICATOR {
    "__sys_socket", "__x64_sys_socket", "sock_create", "__sys_bind", "__x64_sys_bind", "__x64_sys_connect", "__sys_connect",
    "tcp_release_cb", "tcp_init_sock", "tcp_init_xmit_timers", "tcp_v4_connect", "ip_route_output_key_hash", "tcp_connect",
    "tcp_fastopen_defer_connect", "ipv4_dst_check", "tcp_sync_mss", "tcp_initialize_rcv_mss", "tcp_write_queue_purge",
    "tcp_clear_retrans", "tcp_transmit_skb", "__tcp_transmit_skb", "tcp_v4_send_check", "__tcp_v4_send_check", "ip_queue_xmit",
    "__ip_queue_xmit", "ip_local_out", "__ip_local_out", "ip_output", "__ip_finish_output", "dev_queue_xmit", "__dev_queue_xmit",
    "skb_network_protocol", "eth_type_vlan", "netdev_start_xmit"
};

static const std::set<std::string> I40E_DRIVER_FUNC_INDICATOR {
    "i40e_features_check", "i40e_lan_xmit_frame", "i40e_maybe_stop_tx", "vlan_get_protocol", "dma_map_single_attrs", "dma_map_page_attrs", 
    "i40e_maybe_stop_tx"
};

}; // namespace conf

}; // namespace trace

}; // namespace sim


#endif // SIM_TRACE_CONFIG_VARS_H_