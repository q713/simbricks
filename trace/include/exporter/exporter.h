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

#include <memory>
#include <string>
#include <mutex>
#include <unordered_map>
#include <map>
#include <cassert>
#include <strstream>

#include "opentelemetry/sdk/version/version.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_options.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/batch_span_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h"
#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/trace/scope.h"
#include "opentelemetry/common/timestamp.h"

#include "util/exception.h"
#include "events/events.h"
#include "analytics/span.h"
#include "analytics/trace.h"

#ifndef SIMBRICKS_TRACE_EXPORTER_H_
#define SIMBRICKS_TRACE_EXPORTER_H_

namespace simbricks::trace {

class Exporter {

 protected:
  std::mutex exporter_mutex_;

 public:
  Exporter() = default;

  virtual void StartSpan(std::shared_ptr<EventSpan> to_start) = 0;

  virtual void EndSpan(std::shared_ptr<EventSpan> to_end) = 0;
};

class OtlpSpanExporter : public Exporter {

  int64_t time_offset_ = 0;
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> tracer_;

  // mapping: own_context_id -> opentelemetry_span_context
  std::unordered_map<uint64_t, opentelemetry::trace::SpanContext> context_map_;

  // mapping: own_span_id -> opentelemetry_span
  std::unordered_map<uint64_t, opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>> span_map_;

  using ts_steady = std::chrono::time_point<std::chrono::steady_clock, std::chrono::microseconds>;
  using ts_system = std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds>;

  void InsertNewContext(std::shared_ptr<TraceContext> &custom,
                        opentelemetry::trace::SpanContext &context) {
    throw_if_empty(custom, "InsertNewContext custom is null");
    auto context_id = custom->GetId();
    auto iter = context_map_.insert({context_id, context});
    throw_on(not iter.second, "InsertNewContext could not insert context into map");
  }

  opentelemetry::trace::SpanContext
  GetContext(std::shared_ptr<TraceContext> &context_to_get) {
    throw_if_empty(context_to_get, "GetContext context_to_get is null");
    auto iter = context_map_.find(context_to_get->GetId());
    throw_on(iter == context_map_.end(), "GetContext context was not found");
    return iter->second;
  }

  void InsertNewSpan(std::shared_ptr<EventSpan> &old_span,
                     opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &new_span) {
    throw_if_empty(old_span, "InsertNewSpan old span is null");
    throw_on(not new_span, "InsertNewSpan new_span is null");

    const uint64_t span_id = old_span->GetId();
    auto iter = span_map_.insert({span_id, new_span});
    throw_on(not iter.second, "InsertNewSpan could not insert into span map");
  }

  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>
  GetSpan(std::shared_ptr<EventSpan> &span_to_get) {
    throw_if_empty(span_to_get, "GetSpan span_to_get is null");
    const uint64_t span_id = span_to_get->GetId();
    auto span = span_map_.find(span_id)->second;
    throw_on(not span, "InsertNewSpan span is null");
    return span;
  }

  static int64_t GetNowOffsetMicroseconds() {
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::microseconds>(now);
    auto value = now_ms.time_since_epoch();
    return value.count();
  }

  opentelemetry::common::SteadyTimestamp ToSteadyMicroseconds(uint64_t timestamp) const {
    const ts_steady time_point{std::chrono::microseconds{time_offset_ + (timestamp / 1000 / 1000)}};
    return opentelemetry::common::SteadyTimestamp(time_point);
  }

  opentelemetry::common::SystemTimestamp ToSystemMicroseconds(uint64_t timestamp) const {
    const ts_system time_point{std::chrono::microseconds{time_offset_ + (timestamp / 1000 / 1000)}};
    return opentelemetry::common::SystemTimestamp(time_point);
  }

  std::string get_type_str(std::shared_ptr<Event> event) {
    assert(event and "event is not null");
    std::stringstream sss;
    sss << event->get_type();
    return std::move(sss.str());
  }

  std::string get_type_str(std::shared_ptr<EventSpan> span) {
    assert(span and "event is not null");
    std::stringstream sss;
    sss << span->GetType();
    return std::move(sss.str());
  }

  void add_Event(std::map<std::string, std::string> &attributes, std::shared_ptr<Event> event) {
    assert(event and "event is not null");
    const std::string type = get_type_str(event);
    attributes.insert({"timestamp", std::to_string(event->get_ts())});
    attributes.insert({"parser_ident", std::to_string(event->get_parser_ident())});
    attributes.insert({"parser name", event->get_parser_name()});
    attributes.insert({"type", type});
  }

  void add_SimSendSync(std::map<std::string, std::string> &attributes, std::shared_ptr<SimSendSync> event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
  }

  void add_SimProcInEvent(std::map<std::string, std::string> &attributes, std::shared_ptr<SimProcInEvent> event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
  }

  void add_HostInstr(std::map<std::string, std::string> &attributes, std::shared_ptr<HostInstr> event) {
    assert(event and "event is not null");
    attributes.insert({"pc", std::to_string(event->GetPc())});
  }

  void add_HostCall(std::map<std::string, std::string> &attributes, std::shared_ptr<HostCall> event) {
    assert(event and "event is not null");
    add_HostInstr(attributes, event);
    attributes.insert({"func", *(event->GetFunc())});
    attributes.insert({"comp", *(event->GetComp())});
  }

  void add_HostMmioImRespPoW(std::map<std::string, std::string> &attributes, std::shared_ptr<HostMmioImRespPoW> event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
  }

  void add_HostIdOp(std::map<std::string, std::string> &attributes, std::shared_ptr<HostIdOp> event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"id", std::to_string(event->GetId())});
  }

  void add_HostMmioCR(std::map<std::string, std::string> &attributes, std::shared_ptr<HostMmioCR> event) {
    assert(event and "event is not null");
    add_HostIdOp(attributes, event);
  }

  void add_HostMmioCW(std::map<std::string, std::string> &attributes, std::shared_ptr<HostMmioCW> event) {
    assert(event and "event is not null");
    add_HostIdOp(attributes, event);
  }

  void add_HostAddrSizeOp(std::map<std::string, std::string> &attributes, std::shared_ptr<HostAddrSizeOp> event) {
    assert(event and "event is not null");
    add_HostIdOp(attributes, event);
    attributes.insert({"addr", std::to_string(event->GetAddr())});
    attributes.insert({"size", std::to_string(event->GetSize())});
  }

  void add_HostMmioOp(std::map<std::string, std::string> &attributes, std::shared_ptr<HostMmioOp> event) {
    assert(event and "event is not null");
    add_HostAddrSizeOp(attributes, event);
    attributes.insert({"bar", std::to_string(event->GetBar())});
    attributes.insert({"offset", std::to_string(event->GetOffset())});
  }

  void add_HostMmioR(std::map<std::string, std::string> &attributes, std::shared_ptr<HostMmioR> event) {
    assert(event and "event is not null");
    add_HostMmioOp(attributes, event);
  }

  void add_HostMmioW(std::map<std::string, std::string> &attributes, std::shared_ptr<HostMmioW> event) {
    assert(event and "event is not null");
    add_HostMmioOp(attributes, event);
  }

  void add_HostDmaC(std::map<std::string, std::string> &attributes, std::shared_ptr<HostDmaC> event) {
    assert(event and "event is not null");
    add_HostIdOp(attributes, event);
  }

  void add_HostDmaR(std::map<std::string, std::string> &attributes, std::shared_ptr<HostDmaR> event) {
    assert(event and "event is not null");
    add_HostAddrSizeOp(attributes, event);
  }

  void add_HostDmaW(std::map<std::string, std::string> &attributes, std::shared_ptr<HostDmaW> event) {
    assert(event and "event is not null");
    add_HostAddrSizeOp(attributes, event);
  }

  void add_HostMsiX(std::map<std::string, std::string> &attributes, std::shared_ptr<HostMsiX> event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"vec", std::to_string(event->GetVec())});
  }

  void add_HostConf(std::map<std::string, std::string> &attributes, std::shared_ptr<HostConf> event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"dev", std::to_string(event->GetDev())});
    attributes.insert({"func", std::to_string(event->GetFunc())});
    attributes.insert({"reg", std::to_string(event->GetReg())});
    attributes.insert({"bytes", std::to_string(event->GetBytes())});
    attributes.insert({"data", std::to_string(event->GetBytes())});
    attributes.insert({"is_read", event->IsRead() ? "true" : "false"});
  }

  void add_HostClearInt(std::map<std::string, std::string> &attributes, std::shared_ptr<HostClearInt> event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
  }

  void add_HostPostInt(std::map<std::string, std::string> &attributes, std::shared_ptr<HostPostInt> event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
  }

  void add_HostPciRW(std::map<std::string, std::string> &attributes, std::shared_ptr<HostPciRW> event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"offset", std::to_string(event->GetOffset())});
    attributes.insert({"size", std::to_string(event->GetSize())});
    attributes.insert({"is_read", event->IsRead() ? "true" : "false"});
  }

  void add_NicMsix(std::map<std::string, std::string> &attributes, std::shared_ptr<NicMsix> event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"vec", std::to_string(event->GetVec())});
    attributes.insert({"isX", event->IsX() ? "true" : "false"});
  }

  void add_NicDma(std::map<std::string, std::string> &attributes, std::shared_ptr<NicDma> event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"id", std::to_string(event->GetId())});
    attributes.insert({"addr", std::to_string(event->GetId())});
    attributes.insert({"len", std::to_string(event->GetLen())});
  }

  void add_SetIX(std::map<std::string, std::string> &attributes, std::shared_ptr<SetIX> event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"intr", std::to_string(event->GetIntr())});
  }

  void add_NicDmaI(std::map<std::string, std::string> &attributes, std::shared_ptr<NicDmaI> event) {
    assert(event and "event is not null");
    add_NicDma(attributes, event);
  }

  void add_NicDmaEx(std::map<std::string, std::string> &attributes, std::shared_ptr<NicDmaEx> event) {
    assert(event and "event is not null");
    add_NicDma(attributes, event);
  }

  void add_NicDmaEn(std::map<std::string, std::string> &attributes, std::shared_ptr<NicDmaEn> event) {
    assert(event and "event is not null");
    add_NicDma(attributes, event);
  }

  void add_NicDmaCR(std::map<std::string, std::string> &attributes, std::shared_ptr<NicDmaCR> event) {
    assert(event and "event is not null");
    add_NicDma(attributes, event);
  }

  void add_NicDmaCW(std::map<std::string, std::string> &attributes, std::shared_ptr<NicDmaCW> event) {
    assert(event and "event is not null");
    add_NicDma(attributes, event);
  }

  void add_NicMmio(std::map<std::string, std::string> &attributes, std::shared_ptr<NicMmio> event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"off", std::to_string(event->GetOff())});
    attributes.insert({"len", std::to_string(event->GetLen())});
    attributes.insert({"val", std::to_string(event->GetVal())});
  }

  void add_NicMmioR(std::map<std::string, std::string> &attributes, std::shared_ptr<NicMmioR> event) {
    assert(event and "event is not null");
    add_NicMmio(attributes, event);
  }

  void add_NicMmioW(std::map<std::string, std::string> &attributes, std::shared_ptr<NicMmioW> event) {
    assert(event and "event is not null");
    add_NicMmio(attributes, event);
  }

  void add_NicTrx(std::map<std::string, std::string> &attributes, std::shared_ptr<NicTrx> event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"len", std::to_string(event->GetLen())});
  }

  void add_NicTx(std::map<std::string, std::string> &attributes, std::shared_ptr<NicTx> event) {
    assert(event and "event is not null");
    add_NicTrx(attributes, event);
  }

  void add_NicRx(std::map<std::string, std::string> &attributes, std::shared_ptr<NicRx> event) {
    assert(event and "event is not null");
    add_NicTrx(attributes, event);
    attributes.insert({"port", std::to_string(event->GetPort())});
  }

  opentelemetry::trace::StartSpanOptions get_span_start_opts(std::shared_ptr<EventSpan> span) {
    opentelemetry::trace::StartSpanOptions span_options;
    if (span->HasParent()) {
      auto custom_context = span->GetContext();
      auto open_context = GetContext(custom_context);
      span_options.parent = open_context;
    }
    span_options.start_system_time = ToSystemMicroseconds(span->GetStartingTs());
    span_options.start_steady_time = ToSteadyMicroseconds(span->GetStartingTs());

    return std::move(span_options);
  }

  void end_span(std::shared_ptr<EventSpan> old_span,
                opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &new_span) {
    assert(old_span and "old span is null");
    assert(new_span and "new span is null");
    opentelemetry::trace::EndSpanOptions end_opts;
    end_opts.end_steady_time = ToSteadyMicroseconds(old_span->GetCompletionTs());
    new_span->End(end_opts);
  }

  void add_HostMmioW(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span,
                     std::shared_ptr<HostMmioW> event) {
    const std::string type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_HostMmioW(attributes, event);
    span->AddEvent(type, ToSystemMicroseconds(event->get_ts()), attributes);
  }

  void add_HostMmioR(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span,
                     std::shared_ptr<HostMmioR> event) {
    const std::string type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_HostMmioR(attributes, event);
    span->AddEvent(type, ToSystemMicroseconds(event->get_ts()), attributes);
  }

  void add_HostMmioImRespPoW(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span,
                             std::shared_ptr<HostMmioImRespPoW> event) {
    const std::string type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_HostMmioImRespPoW(attributes, event);
    span->AddEvent(type, ToSystemMicroseconds(event->get_ts()), attributes);
  }

  void add_HostMmioCW(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span,
                      std::shared_ptr<HostMmioCW> event) {
    const std::string type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_HostMmioCW(attributes, event);
    span->AddEvent(type, ToSystemMicroseconds(event->get_ts()), attributes);
  }

  void add_HostMmioCR(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span,
                      std::shared_ptr<HostMmioCR> event) {
    const std::string type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_HostMmioCR(attributes, event);
    span->AddEvent(type, ToSystemMicroseconds(event->get_ts()), attributes);
  }

  void add_HostCall(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span,
                    std::shared_ptr<HostCall> event) {
    auto type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_HostCall(attributes, event);
    span->AddEvent(type, ToSystemMicroseconds(event->get_ts()), attributes);
  }

  void add_HostMsiX(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span,
                    std::shared_ptr<HostMsiX> event) {
    auto type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_HostMsiX(attributes, event);
    span->AddEvent(type, ToSystemMicroseconds(event->get_ts()), attributes);
  }

  void add_HostDmaC(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span,
                    std::shared_ptr<HostDmaC> event) {
    auto type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_HostDmaC(attributes, event);
    span->AddEvent(type, ToSystemMicroseconds(event->get_ts()), attributes);
  }

  void add_HostDmaR(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span,
                    std::shared_ptr<HostDmaR> event) {
    auto type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_HostDmaR(attributes, event);
    span->AddEvent(type, ToSystemMicroseconds(event->get_ts()), attributes);
  }

  void add_HostDmaW(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span,
                    std::shared_ptr<HostDmaW> event) {
    auto type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_HostDmaW(attributes, event);
    span->AddEvent(type, ToSystemMicroseconds(event->get_ts()), attributes);
  }

  void add_HostPostInt(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span,
                       std::shared_ptr<HostPostInt> event) {
    auto type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_HostPostInt(attributes, event);
    span->AddEvent(type, ToSystemMicroseconds(event->get_ts()), attributes);
  }

  void add_HostClearInt(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span,
                        std::shared_ptr<HostClearInt> event) {
    auto type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_HostClearInt(attributes, event);
    span->AddEvent(type, ToSystemMicroseconds(event->get_ts()), attributes);
  }

  void add_NicDmaI(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span, std::shared_ptr<NicDmaI> event) {
    auto type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_NicDmaI(attributes, event);
    span->AddEvent(type, ToSystemMicroseconds(event->get_ts()), attributes);
  }

  void add_NicDmaEx(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span,
                    std::shared_ptr<NicDmaEx> event) {
    auto type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_NicDmaEx(attributes, event);
    span->AddEvent(type, ToSystemMicroseconds(event->get_ts()), attributes);
  }

  void add_NicDmaCW(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span,
                    std::shared_ptr<NicDmaCW> event) {
    auto type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_NicDmaCW(attributes, event);
    span->AddEvent(type, ToSystemMicroseconds(event->get_ts()), attributes);
  }

  void add_NicDmaCR(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span,
                    std::shared_ptr<NicDmaCR> event) {
    auto type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_NicDmaCR(attributes, event);
    span->AddEvent(type, ToSystemMicroseconds(event->get_ts()), attributes);
  }

  void add_NicMmioR(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span,
                    std::shared_ptr<NicMmioR> event) {
    auto type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_NicMmioR(attributes, event);
    span->AddEvent(type, ToSystemMicroseconds(event->get_ts()), attributes);
  }

  void add_NicMmioW(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span,
                    std::shared_ptr<NicMmioW> event) {
    auto type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_NicMmioW(attributes, event);
    span->AddEvent(type, ToSystemMicroseconds(event->get_ts()), attributes);
  }

  void add_NicTx(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span, std::shared_ptr<NicTx> event) {
    auto type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_NicTx(attributes, event);
    span->AddEvent(type, ToSystemMicroseconds(event->get_ts()), attributes);
  }

  void add_NicRx(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span, std::shared_ptr<NicRx> event) {
    auto type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_NicRx(attributes, event);
    span->AddEvent(type, ToSystemMicroseconds(event->get_ts()), attributes);
  }

  void add_NicMsix(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &span, std::shared_ptr<NicMsix> event) {
    auto type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_NicMsix(attributes, event);
    span->AddEvent(type, ToSystemMicroseconds(event->get_ts()), attributes);
  }

  void add_EventSpanAttr(std::map<std::string, std::string> attributes, std::shared_ptr<EventSpan> span) {
    auto span_name = get_type_str(span);
    attributes.insert({"id", std::to_string(span->GetId())});
    attributes.insert({"source id", std::to_string(span->GetSourceId())});
    attributes.insert({"type", span_name});
    attributes.insert({"pending", span->IsPending() ? "true" : "false"});
    auto context = span->GetContext();
    throw_if_empty(context, "add_EventSpanAttr context is null");
    attributes.insert({"trace id", std::to_string(context->GetTraceId())});
  }

  void transform_HostCallSpan(std::shared_ptr<HostCallSpan> &to_transform) {
    auto span_opts = get_span_start_opts(to_transform);
    auto span_name = get_type_str(to_transform);
    std::map<std::string, std::string> attributes;
    add_EventSpanAttr(attributes, to_transform);
    auto call_span = tracer_->StartSpan(span_name, attributes, span_opts);

  }

 public:
  OtlpSpanExporter(std::string &&url, std::string service_name, bool batch_mode, std::string &&lib_name) : Exporter() {
    // create exporter
    opentelemetry::exporter::otlp::OtlpHttpExporterOptions opts;
    opts.url = url;
    auto exporter = opentelemetry::exporter::otlp::OtlpHttpExporterFactory::Create(
        opts);
    //auto exporter = opentelemetry::exporter::trace::OStreamSpanExporterFactory::Create();
    throw_if_empty(exporter, span_exporter_null);

    // create span processor
    std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> processor = nullptr;
    if (batch_mode) {
      const opentelemetry::sdk::trace::BatchSpanProcessorOptions batch_opts;
      processor = opentelemetry::sdk::trace::BatchSpanProcessorFactory::Create(
          std::move(exporter), batch_opts);
    } else {
      processor = opentelemetry::sdk::trace::SimpleSpanProcessorFactory::Create(std::move(exporter));
    }
    throw_if_empty(processor, span_processor_null);

    // TODO: create multiple provider, accessed by string mapping to see different sources within jaeger ui
    // create trace provider
    auto resource_attr = opentelemetry::sdk::resource::ResourceAttributes{
        {"service.name", service_name}
    };
    auto resource = opentelemetry::sdk::resource::Resource::Create(resource_attr);
    std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
        opentelemetry::sdk::trace::TracerProviderFactory::Create(std::move(processor), resource);
    throw_if_empty(provider, trace_provider_null);

    // Set the global trace provider
    opentelemetry::trace::Provider::SetTracerProvider(provider);

    // set tracer and offset
    tracer_ = provider->GetTracer(lib_name, OPENTELEMETRY_SDK_VERSION);
    time_offset_ = GetNowOffsetMicroseconds();
  }

  void StartSpan(std::shared_ptr<EventSpan> to_start) override {
    auto span_opts = get_span_start_opts(to_start);
    auto span_name = get_type_str(to_start);
    std::map<std::string, std::string> attributes;
    add_EventSpanAttr(attributes, to_start);

    auto call_span = tracer_->StartSpan(span_name, attributes, span_opts);
    InsertNewSpan(to_start, call_span);

    auto old_context = to_start->GetContext();
    auto new_context = call_span->GetContext();
    InsertNewContext(old_context, new_context);
  }

  void EndSpan(std::shared_ptr<EventSpan> to_end) override {
    auto span = GetSpan(to_end);

    const size_t amount_events = to_end->GetAmountEvents();
    for (size_t index = 0; index < amount_events; index++) {

      auto action = to_end->GetAt(index);
      throw_if_empty(action, "EndSpan action is null");

      switch (action->get_type()) {
        case EventType::HostCall_t: {
          add_HostCall(span, std::static_pointer_cast<HostCall>(action));
          break;
        }
        case EventType::HostMsiX_t: {
          add_HostMsiX(span, std::static_pointer_cast<HostMsiX>(action));
          break;
        }
        case EventType::HostMmioW_t: {
          add_HostMmioW(span, std::static_pointer_cast<HostMmioW>(action));
          break;
        }
        case EventType::HostMmioR_t: {
          add_HostMmioR(span, std::static_pointer_cast<HostMmioR>(action));
          break;
        }
        case EventType::HostMmioImRespPoW_t: {
          add_HostMmioImRespPoW(span,
                                std::static_pointer_cast<HostMmioImRespPoW>(action));
          break;
        }
        case EventType::HostMmioCW_t: {
          add_HostMmioCW(span, std::static_pointer_cast<HostMmioCW>(action));
          break;
        }
        case EventType::HostMmioCR_t: {
          add_HostMmioCR(span, std::static_pointer_cast<HostMmioCR>(action));
          break;
        }
        case EventType::HostDmaW_t: {
          add_HostDmaW(span, std::static_pointer_cast<HostDmaW>(action));
          break;
        }
        case EventType::HostDmaR_t: {
          add_HostDmaR(span, std::static_pointer_cast<HostDmaR>(action));
          break;
        }
        case EventType::HostDmaC_t: {
          add_HostDmaC(span, std::static_pointer_cast<HostDmaC>(action));
          break;
        }
        case EventType::HostPostInt_t: {
          add_HostPostInt(span, std::static_pointer_cast<HostPostInt>(action));
          break;
        }
        case EventType::HostClearInt_t: {
          add_HostClearInt(span, std::static_pointer_cast<HostClearInt>(action));
          break;
        }
        case EventType::NicDmaI_t: {
          add_NicDmaI(span, std::static_pointer_cast<NicDmaI>(action));
          break;
        }
        case EventType::NicDmaEx_t: {
          add_NicDmaEx(span, std::static_pointer_cast<NicDmaEx>(action));
          break;
        }
        case EventType::NicDmaCW_t: {
          add_NicDmaCW(span, std::static_pointer_cast<NicDmaCW>(action));
          break;
        }
        case EventType::NicDmaCR_t: {
          add_NicDmaCR(span, std::static_pointer_cast<NicDmaCR>(action));
          break;
        }
        case EventType::NicMmioR_t: {
          add_NicMmioR(span, std::static_pointer_cast<NicMmioR>(action));
          break;
        }
        case EventType::NicMmioW_t: {
          add_NicMmioW(span, std::static_pointer_cast<NicMmioW>(action));
          break;
        }
        case EventType::NicRx_t: {
          add_NicRx(span, std::static_pointer_cast<NicRx>(action));
          break;
        }
        case EventType::NicTx_t: {
          add_NicTx(span, std::static_pointer_cast<NicTx>(action));
          break;
        }
        case EventType::NicMsix_t: {
          add_NicMsix(span, std::static_pointer_cast<NicMsix>(action));
          break;
        }
        default: {
          throw_just("transform_HostCallSpan unexpected event: ", *action);
        }
      }
    }

    end_span(to_end, span);
  }

  /*
  void transform_HostMsixSpan(std::shared_ptr<HostMsixSpan> &to_transform) {
    auto span_opts = get_span_start_opts(to_transform);
    auto span_name = get_type_str(to_transform);
    std::map<std::string, std::string> attributes;
    add_EventSpanAttr(attributes, to_transform);
    auto msix_span = tracer_->StartSpan(span_name, attributes, span_opts);

    for (auto &event : to_transform->events_) {
      switch (event->get_type()) {
        case EventType::HostMsiX_t:add_HostMsiX(msix_span, std::static_pointer_cast<HostMsiX>(event));
          break;
        case EventType::HostDmaC_t:add_HostDmaC(msix_span, std::static_pointer_cast<HostDmaC>(event));
          break;
        default:throw_just("transform_HostMsixSpan unexpected event: ", *event);
      }
    }

    end_span(to_transform, msix_span);
  }

  void transform_HostMmioSpan(std::shared_ptr<HostMmioSpan> &to_transform) {
    auto span_options = get_span_start_opts(to_transform);
    auto span_name = get_type_str(to_transform);
    std::map<std::string, std::string> attributes;
    add_EventSpanAttr(attributes, to_transform);
    auto mmio_span = tracer_->StartSpan(span_name, attributes, span_options);

    for (auto &event : to_transform->events_) {
      switch (event->get_type()) {
        case EventType::HostMmioW_t:add_HostMmioW(mmio_span, std::static_pointer_cast<HostMmioW>(event));
          break;
        case EventType::HostMmioR_t:add_HostMmioR(mmio_span, std::static_pointer_cast<HostMmioR>(event));
          break;

        case EventType::HostMmioImRespPoW_t:
          add_HostMmioImRespPoW(mmio_span,
                                std::static_pointer_cast<HostMmioImRespPoW>(event));
          break;

        case EventType::HostMmioCW_t:add_HostMmioCW(mmio_span, std::static_pointer_cast<HostMmioCW>(event));
          break;
        case EventType::HostMmioCR_t:add_HostMmioCR(mmio_span, std::static_pointer_cast<HostMmioCR>(event));
          break;

        default:throw_just("transform_HostMmioSpan unexpected event: ", *event);
      }
    }

    end_span(to_transform, mmio_span);
  }

  void transform_HostDmaSpan(std::shared_ptr<HostDmaSpan> &to_transform) {
    auto span_opts = get_span_start_opts(to_transform);
    auto span_name = get_type_str(to_transform);
    std::map<std::string, std::string> attributes;
    add_EventSpanAttr(attributes, to_transform);
    auto dma_span = tracer_->StartSpan(span_name, attributes, span_opts);

    for (auto &event : to_transform->events_) {
      switch (event->get_type()) {
        case EventType::HostDmaW_t:add_HostDmaW(dma_span, std::static_pointer_cast<HostDmaW>(event));
          break;

        case EventType::HostDmaR_t:add_HostDmaR(dma_span, std::static_pointer_cast<HostDmaR>(event));
          break;

        case EventType::HostDmaC_t:add_HostDmaC(dma_span, std::static_pointer_cast<HostDmaC>(event));
          break;

        default:throw_just("transform_HostMsixSpan unexpected event: ", *event);
      }
    }

    end_span(to_transform, dma_span);
  }

  void transform_HostIntSpan(std::shared_ptr<HostIntSpan> &to_transform) {
    auto span_opts = get_span_start_opts(to_transform);
    auto span_name = get_type_str(to_transform);
    std::map<std::string, std::string> attributes;
    add_EventSpanAttr(attributes, to_transform);
    auto int_span = tracer_->StartSpan(span_name, attributes, span_opts);

    for (auto &event : to_transform->events_) {
      switch (event->get_type()) {
        case EventType::HostPostInt_t:add_HostPostInt(int_span, std::static_pointer_cast<HostPostInt>(event));
          break;
        case EventType::HostClearInt_t:add_HostClearInt(int_span, std::static_pointer_cast<HostClearInt>(event));
          break;
        default:throw_just("transform_HostIntSpan unexpected event: ", *event);
      }
    }

    end_span(to_transform, int_span);
  }

  void transform_NicDmaSpan(std::shared_ptr<NicDmaSpan> &to_transform) {
    auto span_opts = get_span_start_opts(to_transform);
    auto span_name = get_type_str(to_transform);
    std::map<std::string, std::string> attributes;
    add_EventSpanAttr(attributes, to_transform);
    auto dma_span = tracer_->StartSpan(span_name, attributes, span_opts);

    for (auto &event : to_transform->events_) {
      switch (event->get_type()) {
        case EventType::NicDmaI_t:add_NicDmaI(dma_span, std::static_pointer_cast<NicDmaI>(event));
          break;
        case EventType::NicDmaEx_t:add_NicDmaEx(dma_span, std::static_pointer_cast<NicDmaEx>(event));
          break;
        case EventType::NicDmaCW_t:add_NicDmaCW(dma_span, std::static_pointer_cast<NicDmaCW>(event));
          break;
        case EventType::NicDmaCR_t:add_NicDmaCR(dma_span, std::static_pointer_cast<NicDmaCR>(event));
          break;
        default:throw_just("transform_NicDmaSpan unexpected event: ", *event);
      }
    }

    end_span(to_transform, dma_span);
  }

  void transform_NicMmioSpan(std::shared_ptr<NicMmioSpan> &to_transform) {
    auto span_opts = get_span_start_opts(to_transform);
    auto span_name = get_type_str(to_transform);
    std::map<std::string, std::string> attributes;
    add_EventSpanAttr(attributes, to_transform);
    auto mmio_span = tracer_->StartSpan(span_name, attributes, span_opts);

    auto action = to_transform->action_;
    if (is_type(action, EventType::NicMmioR_t)) {
      add_NicMmioR(mmio_span, std::static_pointer_cast<NicMmioR>(action));
    } else if (is_type(action, EventType::NicMmioW_t)) {
      add_NicMmioW(mmio_span, std::static_pointer_cast<NicMmioW>(action));
    } else {
      throw_just("transform_NicMmioSpan unexpected event: ", *action);
    }

    end_span(to_transform, mmio_span);
  }

  void transform_NicEthSpan(std::shared_ptr<NicEthSpan> &to_transform) {
    auto span_opts = get_span_start_opts(to_transform);
    auto span_name = get_type_str(to_transform);
    std::map<std::string, std::string> attributes;
    add_EventSpanAttr(attributes, to_transform);
    auto eth_span = tracer_->StartSpan(span_name, attributes, span_opts);

    auto action = to_transform->tx_rx_;
    if (is_type(action, EventType::NicRx_t)) {
      add_NicRx(eth_span, std::static_pointer_cast<NicRx>(action));
    } else if (is_type(action, EventType::NicTx_t)) {
      add_NicTx(eth_span, std::static_pointer_cast<NicTx>(action));
    } else {
      throw_just("transform_NicEthSpan unexpected event: ", *action);
    }

    end_span(to_transform, eth_span);
  }

  void transform_NicMsixSpan(std::shared_ptr<NicMsixSpan> &to_transform) {
    auto span_opts = get_span_start_opts(to_transform);
    auto span_name = get_type_str(to_transform);
    std::map<std::string, std::string> attributes;
    add_EventSpanAttr(attributes, to_transform);
    auto msix_span = tracer_->StartSpan(span_name, attributes, span_opts);

    auto action = to_transform->nic_msix_;
    if (is_type(action, EventType::NicMsix_t)) {
      add_NicMsix(msix_span, std::static_pointer_cast<NicMsix>(action));
    } else {
      throw_just("transform_NicMsixSpan unexpected event: ", *action);
    }

    end_span(to_transform, msix_span);
  }

  void transform_GenericSingleSpan(std::shared_ptr<GenericSingleSpan> &to_transform) {
    auto span_opts = get_span_start_opts(to_transform);
    auto span_name = get_type_str(to_transform);
    std::map<std::string, std::string> attributes;
    add_EventSpanAttr(attributes, to_transform);
    auto span = tracer_->StartSpan(span_name, attributes, span_opts);

    auto action = to_transform->event_p_;
    switch (action->get_type()) {
      case EventType::HostCall_t:add_HostCall(span, std::static_pointer_cast<HostCall>(action));
        break;
      case EventType::HostMsiX_t:add_HostMsiX(span, std::static_pointer_cast<HostMsiX>(action));
        break;
      case EventType::HostMmioW_t:add_HostMmioW(span, std::static_pointer_cast<HostMmioW>(action));
        break;
      case EventType::HostMmioR_t:add_HostMmioR(span, std::static_pointer_cast<HostMmioR>(action));
        break;
      case EventType::HostMmioImRespPoW_t:
        add_HostMmioImRespPoW(span,
                              std::static_pointer_cast<HostMmioImRespPoW>(action));
        break;
      case EventType::HostMmioCW_t:add_HostMmioCW(span, std::static_pointer_cast<HostMmioCW>(action));
        break;
      case EventType::HostMmioCR_t:add_HostMmioCR(span, std::static_pointer_cast<HostMmioCR>(action));
        break;
      case EventType::HostDmaW_t:add_HostDmaW(span, std::static_pointer_cast<HostDmaW>(action));
        break;
      case EventType::HostDmaR_t:add_HostDmaR(span, std::static_pointer_cast<HostDmaR>(action));
        break;
      case EventType::HostDmaC_t:add_HostDmaC(span, std::static_pointer_cast<HostDmaC>(action));
        break;
      case EventType::HostPostInt_t:add_HostPostInt(span, std::static_pointer_cast<HostPostInt>(action));
        break;
      case EventType::HostClearInt_t:add_HostClearInt(span, std::static_pointer_cast<HostClearInt>(action));
        break;
      case EventType::NicDmaI_t:add_NicDmaI(span, std::static_pointer_cast<NicDmaI>(action));
        break;
      case EventType::NicDmaEx_t:add_NicDmaEx(span, std::static_pointer_cast<NicDmaEx>(action));
        break;
      case EventType::NicDmaCW_t:add_NicDmaCW(span, std::static_pointer_cast<NicDmaCW>(action));
        break;
      case EventType::NicDmaCR_t:add_NicDmaCR(span, std::static_pointer_cast<NicDmaCR>(action));
        break;
      case EventType::NicMmioR_t:add_NicMmioR(span, std::static_pointer_cast<NicMmioR>(action));
        break;
      case EventType::NicMmioW_t:add_NicMmioW(span, std::static_pointer_cast<NicMmioW>(action));
        break;
      case EventType::NicRx_t:add_NicRx(span, std::static_pointer_cast<NicRx>(action));
        break;
      case EventType::NicTx_t:add_NicTx(span, std::static_pointer_cast<NicTx>(action));
        break;
      case EventType::NicMsix_t:add_NicMsix(span, std::static_pointer_cast<NicMsix>(action));
        break;
      default:throw_just("transform_GenericSingleSpan unexpected event: ", *action);
    }

    end_span(to_transform, span);
  }

 public:


  ~OtlpSpanExporter() {
    auto provider = opentelemetry::trace::Provider::GetTracerProvider();
    if (provider) {
      static_cast<opentelemetry::sdk::trace::TracerProvider *>(provider.get())->ForceFlush();
    }
    const std::shared_ptr<opentelemetry::trace::TracerProvider> none;
    opentelemetry::trace::Provider::SetTracerProvider(none);
  }

  void StartSpan(std::shared_ptr<EventSpan> span) override {
    const std::lock_guard<std::mutex> lock_guard(exporter_mutex_);

    throw_on(span->IsPending(), "span to export is not yet done");

    switch (span->GetType()) {
      case span_type::kHostCall: {
        auto host_call = std::static_pointer_cast<HostCallSpan>(span);
        start_HostCallSpan(host_call);
        break;
      }
      case span_type::kHostMsix: {
        auto host_msix = std::static_pointer_cast<HostMsixSpan>(span);
        start_HostMsixSpan(host_msix);
        break;
      }
      case span_type::kHostMmio: {
        auto host_mmio = std::static_pointer_cast<HostMmioSpan>(span);
        start_HostMmioSpan(host_mmio);
        break;
      }
      case span_type::kHostDma: {
        auto host_dma = std::static_pointer_cast<HostDmaSpan>(span);
        start_HostDmaSpan(host_dma);
        break;
      }
      case span_type::kHostInt: {
        auto host_int = std::static_pointer_cast<HostIntSpan>(span);
        start_HostIntSpan(host_int);
        break;
      }
      case span_type::kHostDma: {
        auto nic_dma = std::static_pointer_cast<NicDmaSpan>(span);
        start_NicDmaSpan(nic_dma);
        break;
      }
      case span_type::kNicMmio: {
        auto nic_mmio = std::static_pointer_cast<NicMmioSpan>(span);
        start_NicMmioSpan(nic_mmio);
        break;
      }
      case span_type::kNicEth: {
        auto nic_eth = std::static_pointer_cast<NicEthSpan>(span);
        start_NicEthSpan(nic_eth);
        break;
      }
      case span_type::kNicMsix: {
        auto nic_msix = std::static_pointer_cast<NicMsixSpan>(span);
        start_NicMsixSpan(nic_msix);
        break;
      }
      case span_type::kGenericSingle: {
        auto generic_single = std::static_pointer_cast<GenericSingleSpan>(span);
        start_GenericSingleSpan(generic_single);
        break;
      }
      default:std::stringstream tss;
        tss << span->GetType();
        throw_just("span to export has unknown span type: ", tss.str());
    }
  }

  void EndSpan(std::shared_ptr<EventSpan> span) override {
    const std::lock_guard<std::mutex> lock_guard(exporter_mutex_);

    throw_on(span->IsPending(), "span to export is not yet done");

    switch (span->GetType()) {
      case span_type::kHostCall: {
        auto host_call = std::static_pointer_cast<HostCallSpan>(span);
        end_HostCallSpan(host_call);
        break;
      }
      case span_type::kHostMsix: {
        auto host_msix = std::static_pointer_cast<HostMsixSpan>(span);
        end_HostMsixSpan(host_msix);
        break;
      }
      case span_type::kHostMmio: {
        auto host_mmio = std::static_pointer_cast<HostMmioSpan>(span);
        end_HostMmioSpan(host_mmio);
        break;
      }
      case span_type::kHostDma: {
        auto host_dma = std::static_pointer_cast<HostDmaSpan>(span);
        end_HostDmaSpan(host_dma);
        break;
      }
      case span_type::kHostInt: {
        auto host_int = std::static_pointer_cast<HostIntSpan>(span);
        end_HostIntSpan(host_int);
        break;
      }
      case span_type::kHostDma: {
        auto nic_dma = std::static_pointer_cast<NicDmaSpan>(span);
        end_NicDmaSpan(nic_dma);
        break;
      }
      case span_type::kNicMmio: {
        auto nic_mmio = std::static_pointer_cast<NicMmioSpan>(span);
        end_NicMmioSpan(nic_mmio);
        break;
      }
      case span_type::kNicEth: {
        auto nic_eth = std::static_pointer_cast<NicEthSpan>(span);
        end_NicEthSpan(nic_eth);
        break;
      }
      case span_type::kNicMsix: {
        auto nic_msix = std::static_pointer_cast<NicMsixSpan>(span);
        end_NicMsixSpan(nic_msix);
        break;
      }
      case span_type::kGenericSingle: {
        auto generic_single = std::static_pointer_cast<GenericSingleSpan>(span);
        end_GenericSingleSpan(generic_single);
        break;
      }
      default:std::stringstream tss;
        tss << span->GetType();
        throw_just("span to export has unknown span type: ", tss.str());
    }
  }*/
};

}  // namespace simbricks::trace

#endif //SIMBRICKS_TRACE_EXPORTER_H_
