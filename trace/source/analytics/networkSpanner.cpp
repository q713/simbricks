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
    co_return false;
  }
  auto network_event = std::static_pointer_cast<NetworkEvent>(event_ptr);

  if (current_device_span_ and current_device_span_->AddToSpan(network_event)) {
    // we have a pending span, here we complete it
    if (current_device_span_->IsComplete()) {
      // this should always be true -> may remove if statement
      tracer_.MarkSpanAsDone(current_device_span_);
    }
    co_return true;
  }

  // we need to create a new span
  if (network_event->IsBoundaryType(NetworkEvent::EventBoundaryType::kFromAdapter)) {
    // is it a fromAdapter event, we need to poll from the host queue to get the parent
    throw_on(current_device_span_ and current_device_span_->IsPending(),
             "NetworkSpanner::HandelNetworkEvent : current device span is still pending",
             source_loc::current());

    auto con_opt = co_await from_host_->Pop(resume_executor);
    auto con = OrElseThrow(con_opt, TraceException::kContextIsNull,
                           source_loc::current());
    throw_if_empty(con, TraceException::kContextIsNull, source_loc::current());
    throw_on(not is_expectation(con, expectation::kRx),
             "received non kRx context", source_loc::current());

    current_device_span_ = tracer_.StartSpanByParentPassOnContext<NetDeviceSpan>(
        con, network_event, network_event->GetParserIdent(), name_);
    throw_if_empty(current_device_span_, TraceException::kEventIsNull, source_loc::current());
    co_return true;
  }

  // otherwise we need to connect the new created span to the current_device_span
  throw_if_empty(current_device_span_, TraceException::kSpanIsNull, source_loc::current());
  current_device_span_ = tracer_.StartSpanByParent<NetDeviceSpan>(current_device_span_,
                                                                  network_event,
                                                                  network_event->GetParserIdent(),
                                                                  name_);
  throw_if_empty(current_device_span_, TraceException::kSpanIsNull, source_loc::current());

  if (network_event->IsBoundaryType(NetworkEvent::EventBoundaryType::kToAdapter)) {
    // if we have not a "to" adapter event we need to push a context to a host
    auto context = Context::CreatePassOnContext<expectation::kRx>(current_device_span_);
    throw_if_empty(context, TraceException::kContextIsNull, source_loc::current());

    bool could_push = co_await to_host_->Push(resume_executor, context);
    throw_on_false(could_push, TraceException::kCouldNotPushToContextQueue, source_loc::current());
  }

  co_return true;
}

NetworkSpanner::NetworkSpanner(TraceEnvironment &trace_environment,
                               std::string &&name,
                               Tracer &tra,
                               ChannelT from_host,
                               ChannelT to_host)
    : Spanner(trace_environment, std::move(name), tra), from_host_(std::move(from_host)), to_host_(std::move(to_host)) {
  throw_if_empty(from_host_, TraceException::kQueueIsNull, source_loc::current());
  throw_if_empty(to_host_, TraceException::kQueueIsNull, source_loc::current());

  auto handel_net_ev = [this](
      std::shared_ptr<concurrencpp::executor> resume_executor,
      EventT &event_ptr) {
    return HandelNetworkEvent(std::move(resume_executor), event_ptr);
  };

  RegisterHandler(EventType::kNetworkEnqueueT, handel_net_ev);
  RegisterHandler(EventType::kNetworkDequeueT, handel_net_ev);
  RegisterHandler(EventType::kNetworkDropT, handel_net_ev);
}
