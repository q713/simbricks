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

#include <utility>

#include "analytics/spanner.h"

concurrencpp::lazy_result<bool> NetworkSpanner::HandelNetworkEvent(std::shared_ptr<concurrencpp::executor> resume_executor,
                                                                   std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  if (not IsAnyType(event_ptr, std::vector<EventType>{
      EventType::kNetworkEnqueueT, EventType::kNetworkDequeueT, EventType::kNetworkDropT})) {
    // std::cout << "NetworkSpanner::HandelNetworkEvent wrong event type: " << event_ptr << '\n';
    co_return false;
  }
  auto network_event = std::static_pointer_cast<NetworkEvent>(event_ptr);

  // Handling events caused by messages marked as interesting that are not! (ARP)
  //       ---> filter out spans and events that end up in devices we are not interested in!
  if (network_event->InterestingFlag() and node_device_filter_.IsNotInterestingNodeDevice(network_event)) {
    std::cout << "NetworkSpanner::HandelNetworkEvent filtered interesting event because of node device: "
              << network_event << '\n';
    co_return true;
  }

  if (current_device_span_ and current_device_span_->AddToSpan(network_event)) {

    throw_on_false(current_device_span_->IsComplete(),
                   "network spanner, after adding event, span must be complete",
                   source_loc::current());

    // we make the connection at the end to sending NIC
    if (current_device_span_->ContainsBoundaryType(NetworkEvent::EventBoundaryType::kToAdapter)
        and not current_device_span_->IsDrop()) {

      // if we have a "to" adapter event we need to push a context to a host
      auto context = Context::CreatePassOnContext<expectation::kRx>(current_device_span_);
      throw_if_empty(context, TraceException::kContextIsNull, source_loc::current());

      throw_on_false(current_device_span_->HasIpsSet(), "kToAdapter event has no ip header",
                     source_loc::current());

      const int node = current_device_span_->GetNode();
      const int device = current_device_span_->GetDevice();
      auto to_host = to_host_channels_.GetValidChannel(node, device);
      //std::cout << "NetworkSpanner::HandelNetworkEvent: try push kToAdapter context event="
      //          << event_ptr << std::endl;
      bool could_push = co_await to_host->Push(resume_executor, context);
      //std::cout << "NetworkSpanner::HandelNetworkEvent: did push kToAdapter context" << event_ptr << std::endl;
      throw_on_false(could_push, TraceException::kCouldNotPushToContextQueue, source_loc::current());

    }

    last_finished_device_span_ = current_device_span_;
    tracer_.MarkSpanAsDone(current_device_span_);
    co_return true;
  }

  // this can happen due to the interestingness (ARP) issues...
  if (not IsType(network_event, EventType::kNetworkEnqueueT)) {
    co_return false;
  }

  throw_on(current_device_span_ and current_device_span_->IsPending(),
           "NetworkSpanner::HandelNetworkEvent : current device span is still pending",
           source_loc::current());

  // Handling events caused by messages started by not interesting devices (ARP)
  //       ---> in case a span is not interesting but ends up in an interesting device, start a new trace...
  if (not network_event->InterestingFlag()) {
    if (node_device_filter_.IsInterestingNodeDevice(network_event)) {
      //std::cout << "NetworkSpanner::HandelNetworkEvent: started new trace by network event, arp?!" << std::endl;
      current_device_span_ = tracer_.StartSpan<NetDeviceSpan>(network_event,
                                                              network_event->GetParserIdent(),
                                                              name_);
      throw_if_empty(current_device_span_, TraceException::kSpanIsNull, source_loc::current());
      co_return true;
    }
    //std::cout
    //    << "NetworkSpanner::HandelNetworkEvent filtered non interesting potentially starting trace event because of node device: "
    //    << network_event << '\n';
    co_return true;
  }

  std::shared_ptr<Context> context_to_connect_with;
  if (IsBoundaryType(network_event, NetworkEvent::EventBoundaryType::kFromAdapter)) {
    throw_on_false(IsDeviceType(network_event, NetworkEvent::NetworkDeviceType::kCosimNetDevice),
                   "trying to create a span depending on a nic side event based on a non cosim device",
                   source_loc::current());
    // is it a fromAdapter event, we need to poll from the host queue to get the parent
    //std::cout << "NetworkSpanner::HandelNetworkEvent: try pop kFromAdapter context " << event_ptr << std::endl;
    auto con_opt = co_await from_host_->Pop(resume_executor);
    //std::cout << "NetworkSpanner::HandelNetworkEvent: succesfull pop kFromAdapter context" << std::endl;
    context_to_connect_with = OrElseThrow(con_opt, TraceException::kContextIsNull,
                                          source_loc::current());
    throw_if_empty(context_to_connect_with, TraceException::kContextIsNull, source_loc::current());
    throw_on(not is_expectation(context_to_connect_with, expectation::kRx),
             "received non kRx context", source_loc::current());
  } else {
    assert(last_finished_device_span_);
    context_to_connect_with = Context::CreatePassOnContext<expectation::kRx>(last_finished_device_span_);
  }
  assert(context_to_connect_with);

  current_device_span_ = tracer_.StartSpanByParentPassOnContext<NetDeviceSpan>(context_to_connect_with,
                                                                               network_event,
                                                                               network_event->GetParserIdent(),
                                                                               name_);
  throw_if_empty(current_device_span_, TraceException::kSpanIsNull, source_loc::current());
  co_return true;
}

NetworkSpanner::NetworkSpanner(TraceEnvironment &trace_environment,
                               std::string &&name,
                               Tracer &tra,
                               ChannelT from_host,
                               const NodeDeviceToChannelMap &to_host_channels,
                               const NodeDeviceFilter &node_device_filter)
    : Spanner(trace_environment, std::move(name), tra),
      from_host_(std::move(from_host)),
      to_host_channels_(to_host_channels),
      node_device_filter_(node_device_filter) {
  throw_if_empty(from_host_, TraceException::kQueueIsNull, source_loc::current());

  auto handel_net_ev = [this](
      std::shared_ptr<concurrencpp::executor> resume_executor,
      EventT &event_ptr) {
    return HandelNetworkEvent(std::move(resume_executor), event_ptr);
  };

  RegisterHandler(EventType::kNetworkEnqueueT, handel_net_ev);
  RegisterHandler(EventType::kNetworkDequeueT, handel_net_ev);
  RegisterHandler(EventType::kNetworkDropT, handel_net_ev);
}
