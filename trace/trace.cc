/*
 * Copyright 2022 Max Planck Institute for Software Systems, and
 * National University of Singapore
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "lib/utils/cxxopts.hpp"
#include "lib/utils/log.h"
#include "trace/parser/parser.h"
#include "trace/events/eventStreamOperators.h"

int main(int argc, char *argv[]) {
  cxxopts::Options options("trace", "Log File Analysis/Tracing Tool");
  options.add_options()("h,help", "Print usage")(
      "linux-dump-server-client",
      "file path to a output file obtained by 'objdump -S linux_image'",
      cxxopts::value<std::string>())(
      "nic-i40e-dump",
      "file path to a output file obtained by 'objdump -d i40e.ko' (driver)",
      cxxopts::value<std::string>())(
      "gem5-log-server", "file path to a server log file written by gem5",
      cxxopts::value<std::string>())(
      "nicbm-log-server", "file path to a server log file written by the nicbm",
      cxxopts::value<std::string>())(
      "gem5-log-client", "file path to a client log file written by gem5",
      cxxopts::value<std::string>())(
      "nicbm-log-client", "file path to a client log file written by the nicbm",
      cxxopts::value<std::string>())("ts-lower-bound",
                                     "lower timestamp bound for events",
                                     cxxopts::value<std::string>())(
      "ts-upper-bound", "upper timestamp bound for events",
      cxxopts::value<std::string>());

  cxxopts::ParseResult result;
  try {
    result = options.parse(argc, argv);

  } catch (cxxopts::exceptions::exception &e) {
    DFLOGERR("Could not parse cli options: %s\n", e.what());
    exit(EXIT_FAILURE);
  }

  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    exit(EXIT_SUCCESS);
  }

  if (!result.count("linux-dump-server-client") ||
      !result.count("gem5-log-server") || !result.count("nicbm-log-server") ||
      !result.count("gem5-log-client") || !result.count("nicbm-log-client")) {
    std::cerr << "invalid arguments given" << std::endl
              << options.help() << std::endl;
    exit(EXIT_FAILURE);
  }

  // symbol filter to translate hex address to function-name/label
  LineReader ssymsLr;
  SSyms syms_filter{"SymbolTable-Client-Server",
                    ssymsLr};/*,
                     {"entry_SYSCALL_64",
                      "__sys_sendto",
                      "__sys_recvfrom",
                      "__sock_sendmsg",
                      "sock_queue_rcv_skb",
                      "__security_sock_sendmsg",
                      "syscall_return_via_sysret",
                      "write",
                      "sendto",
                      "sendmsg",
                      "deliver_skb",
                      "tcp_sendmsg",
                      "tcp_transmit_skb",
                      "ip_queue_xmit",
                      "ip_output",
                      "ip_rt_do_proc_exit",
                      "ip_rt_do_proc_init",
                      "ip_rcv",
                      "tun_napi_poll",
                      "napi_tx",
                      "napi_gro_receive",
                      "napi_schedule_prep",
                      "trace_napi_poll",
                      "perf_trace_napi_poll",
                      "netif_napi_add",
                      "napi_busy_loop",
                      "__napi_schedule_",
                      "napi_complete_done",
                      "napi_schedule_prep",
                      "__napi_schedule",
                      "net_rx_action",
                      "dev_queue_xmit",
                      "interrupt_entry",
                      "__do_softirq",
                      "net_rx_action",
                      "activate_task",
                      "exit_to_user_mode_prepare",
                      "write",
                      "sendto",
                      "sendmsg",
                      "__x64_sys_write",
                      "sock_sendmsg",
                      "__sock_sendmsg",
                      "security_socket_sendmsg",
                      "tcp_sendmsg",
                      "tcp_sendmsg_locked",
                      "tcp_rate_check_app_limited",
                      "tcp_send_mss",
                      "tcp_current_mss",
                      "tcp_stream_memory_free",
                      "sk_stream_alloc_skb",
                      "tcp_skb_entail",
                      "sk_page_frag_refill",
                      "tcp_tx_timestamp",
                      "tcp_push",
                      "__tcp_push_pending_frames",
                      "tcp_write_xmit",
                      "tcp_transmit_skb",
                      "__tcp_transmit_skb",
                      "tcp_established_options",
                      "skb_push",
                      "__tcp_select_window",
                      "tcp_options_write",
                      "tcp_v4_send_check",
                      "queue_xmit",
                      "ip_queue_xmit",
                      "__ip_queue_xmit",
                      "__sk_dst_check",
                      "skb_push",
                      "ip_send_check",
                      "ip_output",
                      "ip_finish_output",
                      "__ip_finish_output",
                      "dev_queue_xmit",
                      "__dev_queue_xmit",
                      "skb_network_protocol",
                      "netif_skb_features",
                      "dev_hard_start_xmit",
                      "skb_clone_tx_timestamp",
                      "tcp_wfree",
                      "eth_type_trans",
                      "netif_rx",
                      "netif_rx_internal",
                      "__netif_receive_skb",
                      "__netif_receive_skb_one_core",
                      "ip_rcv",
                      "ip_rcv_core",
                      "tcp_v4_rcv",
                      "tcp_filter",
                      "tcp_queue_rcv",
                      "tcp_event_data_recv",
                      "__tcp_ack_snd_check",
                      "tcp_send_ack",
                      "__tcp_transmit_skb",
                      "netif_skb_features",
                      "loopback_xmit",
                      "skb_clone_tx_timestamp",
                      "__sock_wfree",
                      "eth_type_trans",
                      "tcp_data_ready",
                      "__netif_receive_skb",
                      "__netif_receive_skb_one_core",
                      "ip_rcv",
                      "ip_rcv_core",
                      "tcp_v4_rcv",
                      "tcp_filter",
                      "tcp_v4_fill_cb",
                      "tcp_update_skb_after_send",
                      "tcp_event_new_data_sent",
                      "tcp_rbtree_insert",
                      "tcp_rearm_rto",
                      "tcp_chrono_stop",
                      "release_sock",
                      "tcp_v4_do_rcv",
                      "tcp_rcv_established",
                      "tcp_ack",
                      "tcp_update_pacing_rate",
                      "tcp_xmit_recovery",
                      "__kfree_skb",
                      "skb_release_head_state",
                      "skb_release_data",
                      "kfree_skbmem",
                      "tcp_check_space",
                      "tcp_release_cb",
                      "syscall_exit_work",
                      "netif_rx_schedule",
                      "net_rx_action",
                      "netif_receive_skb",
                      "netif_rx_complete",
                      "netpoll_rx",
                      "handle_ing",
                      "handle_bridge",
                      "handle_macvlan",
                      "deliver_skb",
                      "arp_rcv",
                      "ip_rcv",
                      "ip_route_input",
                      "ip_mr_input",
                      "ip_mr_forward",
                      "ip_local_delivery",
                      "ip_forward",
                      "ip_local_deliver",
                      "raw_local_deliver",
                      "tcp_v4_rcv",
                      "tcp_data_snd_check",
                      "tcp_ack_snd_check",
                      "tcp_rcv_established",
                      "tcp_data_queue",
                      "sk_receive_queue",
                      "sk_async_wait_queue",
                      "skb_copy_datagram_iovec",
                      "read",
                      "recvfrom",
                      "recvmsg",
                      "__sock_recvmsg",
                      "security_sock_recvmsg",
                      "sock_common_recvmsg",
                      "tcp_recvmsg",
                      "skb_copy_datagram_iovec",
                      "qdisc_run",
                      "qdisc_restart",
                      "netif_schedule",
                      "net_tx_action",
                      "net_tx_action",
                      "qdisc_run",
                      "dev_kfree_skb_irq",
                      "sock_poll",
                      "tcp_poll",
                      "__pollwait",
                      "__x64_sys_read",
                      "ksys_read",
                      "new_sync_read",
                      "sock_recvmsg",
                      "security_socket_recvmsg",
                      "tcp_recvmsg",
                      "tcp_recvmsg_locked",
                      "tcp_rcv_space_adjust",
                      "tcp_update_recv_tstamps",
                      "__kfree_skb",
                      "tcp_cleanup_rbuf",
                      "syscall_exit_work",
                      "__x64_sys_write",
                      "ksys_write",
                      "new_sync_write",
                      "sock_sendmsg",
                      "security_socket_sendmsg",
                      "udp_sendmsg",
                      "dev_queue_xmit",
                      "__dev_queue_xmit",
                      "netdev_core_pick_tx",
                      "validate_xmit_skb",
                      "netif_skb_features",
                      "skb_network_protocol",
                      "validate_xmit_xfrm",
                      "dev_hard_start_xmit",
                      "loopback_xmit",
                      "skb_clone_tx_timestamp",
                      "eth_type_trans",
                      "netif_rx",
                      "netif_rx_internal",
                      "net_rx_action",
                      "__napi_poll",
                      "__netif_receive_skb",
                      "__netif_receive_skb_one_core",
                      "ip_rcv",
                      "ip_rcv_core",
                      "ip_local_deliver",
                      "__rcu_read_lock",
                      "ip_protocol_deliver_rcu",
                      "raw_local_deliver",
                      "udp_rcv",
                      "__udp4_lib_rcv",
                      "udp_unicast_rcv_skb",
                      "udp_queue_rcv_skb",
                      "udp_queue_rcv_one_skb",
                      "ipv4_pktinfo_prepare",
                      "syscall_exit_work",
                      "sk_dst_check",
                      "ipv4_dst_check",
                      "ip_make_skb",
                      "sock_alloc_send_skb",
                      "sock_alloc_send_pskb",
                      "__alloc_skb",
                      "ip_generic_getfrag",
                      "__ip_make_skb",
                      "udp_send_skb",
                      "udp4_hwcsum",
                      "ip_send_skb",
                      "__ip_local_out",
                      "ip_send_check",
                      "ip_output",
                      "ip_finish_output",
                      "__ip_finish_output",
                      "ip_finish_output2",
                      "sock_poll",
                      "udp_poll",
                      "datagram_poll",
                      "__x64_sys_read",
                      "ksys_read",
                      "new_sync_read",
                      "sock_read_iter",
                      "sock_recvmsg",
                      "security_socket_recvmsg",
                      "inet_recvmsg",
                      "udp_recvmsg",
                      "__skb_recv_udp",
                      "__skb_try_recv_from_queue",
                      "udp_rmem_release",
                      "skb_consume_udp",
                      "__consume_stateless_skb",
                      "skb_release_data",
                      "skb_free_head",
                      "kfree_skbmem",
                      "syscall_exit_work"}};*/

  if ((result.count("linux-dump-server-client") &&
       !syms_filter.load_file(
           result["linux-dump-server-client"].as<std::string>(), 0)) ||
      (result.count("nic-i40e-dump") &&
       !syms_filter.load_file(result["nic-i40e-dump"].as<std::string>(),
                              0xffffffffa0000000ULL))) {
    std::cerr << "could not initialize symbol table" << std::endl;
    exit(EXIT_FAILURE);
  }

  /*
   * Parsers
   */

  // component filter to only parse events written from certain components
  ComponentFilter compF("ComponentFilter-Client-Server");

  // gem5 log parser that generates events
  LineReader serverLr;
  Gem5Parser gem5ServerPar{"Gem5ServerParser",
                           result["gem5-log-server"].as<std::string>(),
                           syms_filter, compF, serverLr};

  LineReader clientLr;
  Gem5Parser gem5ClientPar{"Gem5ClientParser",
                           result["gem5-log-client"].as<std::string>(),
                           syms_filter, compF, clientLr};

  // nicbm log parser that generates events
  LineReader nicSerLr;
  NicBmParser nicSerPar{"NicbmServerParser",
                        result["nicbm-log-server"].as<std::string>(), nicSerLr};
  LineReader nicCliLr;
  NicBmParser nicCliPar{"NicbmClientParser",
                        result["nicbm-log-client"].as<std::string>(), nicCliLr};
  
  /*
   * Filter and operators
   */

  // 1475802058125
  // 1596059510250
  uint64_t lower_bound =
      EventTimestampFilter::EventTimeBoundary::MIN_LOWER_BOUND;
  uint64_t upper_bound =
      EventTimestampFilter::EventTimeBoundary::MAX_UPPER_BOUND;

  if (result.count("ts-upper-bound"))
    sim_string_utils::parse_uint_trim_copy(
        result["ts-upper-bound"].as<std::string>(), 10, &upper_bound);

  if (result.count("ts-lower-bound"))
    sim_string_utils::parse_uint_trim_copy(
        result["ts-lower-bound"].as<std::string>(), 10, &lower_bound);

  EventTimestampFilter timestampFilter{
      EventTimestampFilter::EventTimeBoundary{lower_bound, upper_bound}};

  EventTypeStatistics statistics{};

  // filter events out of stream
  EventTypeFilter eventFilter{
      {EventType::HostInstr_t, EventType::SimProcInEvent_t,
       EventType::SimSendSync_t},
      true};

  // printer to consume pipeline and to print events
  EventPrinter eventPrinter;

  using event_t = std::shared_ptr<Event>;

  sim::coroutine::collector<event_t, EventComperator> collector{{nicSerPar, nicCliPar, gem5ServerPar, gem5ClientPar}};

  sim::coroutine::pipeline<event_t> pipeline{collector, {timestampFilter, eventFilter, statistics}};

  if (!sim::coroutine::awaiter<event_t>::await_termination(pipeline, eventPrinter)) {
    std::cerr << "could not await termination of the pipeline" << std::endl;
    exit(EXIT_FAILURE);
  }

  std::cout << statistics << std::endl;

  exit(EXIT_SUCCESS);
}
