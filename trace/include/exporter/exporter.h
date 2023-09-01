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
#include "util/utils.h"

#ifndef SIMBRICKS_TRACE_EXPORTER_H_
#define SIMBRICKS_TRACE_EXPORTER_H_

namespace simbricks::trace {

class SpanExporter {

 protected:
  std::recursive_mutex exporter_mutex_;

 public:
  SpanExporter() = default;

  virtual void StartSpan(std::shared_ptr<EventSpan> to_start) = 0;

  virtual void EndSpan(std::shared_ptr<EventSpan> to_end) = 0;

  virtual void ExportSpan(std::shared_ptr<EventSpan> to_export) = 0;
};

// special span exporter that doies nothing, may be useful for debugging purposes
class NoOpExporter : public SpanExporter {
 public:
  NoOpExporter() = default;

  void StartSpan(std::shared_ptr<EventSpan> to_start) override {
  }

  void EndSpan(std::shared_ptr<EventSpan> to_end) override {
  }

  void ExportSpan(std::shared_ptr<EventSpan> to_export) override {
  }
};

class OtlpSpanExporter : public SpanExporter {

  const int64_t time_offset_;
  const std::string url_;
  bool batch_mode_ = false;
  std::string lib_name_;

  // mapping: own_context_id -> opentelemetry_span_context
  using context_t = opentelemetry::trace::SpanContext;
  std::unordered_map<uint64_t, context_t> context_map_;

  // mapping: own_span_id -> opentelemetry_span
  using span_t = opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>;
  std::unordered_map<uint64_t, span_t> span_map_;

  // service/simulator/spanner name -> otlp_exporter
  using tracer_t = opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>;
  using provider_t = std::shared_ptr<opentelemetry::trace::TracerProvider>;
  std::unordered_map<std::string, tracer_t> tracer_map_;
  std::vector<provider_t> provider_;

  static constexpr int kPicoToNanoDenominator = 1000;
  using ts_steady = std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds>;
  using ts_system = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;

  void InsertNewContext(std::shared_ptr<TraceContext> &custom,
                        context_t &context) {
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
                     span_t &new_span) {
    throw_if_empty(old_span, "InsertNewSpan old span is null");
    throw_on(not new_span, "InsertNewSpan new_span is null");

    const uint64_t span_id = old_span->GetId();
    auto iter = span_map_.insert({span_id, new_span});
    throw_on(not iter.second, "InsertNewSpan could not insert into span map");
  }

  void RemoveSpan(const std::shared_ptr<EventSpan> &old_span) {
    const size_t erased = span_map_.erase(old_span->GetId());
    throw_on(erased != 1, "RemoveSpan did not remove a single span");
  }

  tracer_t CreateTracer(std::string &service_name) {
    // create exporter
    opentelemetry::exporter::otlp::OtlpHttpExporterOptions opts;
    opts.url = url_;
    auto exporter = opentelemetry::exporter::otlp::OtlpHttpExporterFactory::Create(
        opts);
    //auto exporter = opentelemetry::exporter::trace::OStreamSpanExporterFactory::Create();
    throw_if_empty(exporter, span_exporter_null);

    // create span processor
    std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> processor = nullptr;
    if (batch_mode_) {
      const opentelemetry::sdk::trace::BatchSpanProcessorOptions batch_opts;
      processor = opentelemetry::sdk::trace::BatchSpanProcessorFactory::Create(
          std::move(exporter), batch_opts);
    } else {
      processor = opentelemetry::sdk::trace::SimpleSpanProcessorFactory::Create(std::move(exporter));
    }
    throw_if_empty(processor, span_processor_null);

    // create trace provider
    auto resource_attr = opentelemetry::sdk::resource::ResourceAttributes{
        {"service.name", service_name}
    };
    auto resource = opentelemetry::sdk::resource::Resource::Create(resource_attr);
    const provider_t
        provider = opentelemetry::sdk::trace::TracerProviderFactory::Create(std::move(processor), resource);
    throw_if_empty(provider, trace_provider_null);

    // Set the global trace provider
    provider_.push_back(provider);
    //opentelemetry::trace::Provider::SetTracerProvider(provider);

    // set tracer
    auto tracer = provider->GetTracer(lib_name_, OPENTELEMETRY_SDK_VERSION);
    return tracer;
  }

  tracer_t GetTracerLazy(std::string &service_name) {
    auto iter = tracer_map_.find(service_name);
    if (iter != tracer_map_.end()) {
      return iter->second;
    }

    auto tracer = CreateTracer(service_name);
    tracer_map_.insert({service_name, tracer});
    return tracer;
  }

  span_t GetSpan(std::shared_ptr<EventSpan> &span_to_get) {
    throw_if_empty(span_to_get, "GetSpan span_to_get is null");
    const uint64_t span_id = span_to_get->GetId();
    auto span = span_map_.find(span_id)->second;
    throw_on(not span, "InsertNewSpan span is null");
    return span;
  }

  opentelemetry::common::SteadyTimestamp ToSteadyNanoseconds(uint64_t timestamp_pico) const {
    const std::chrono::nanoseconds nano_sec(time_offset_ / kPicoToNanoDenominator + timestamp_pico);
    const ts_steady time_point(nano_sec);
    return opentelemetry::common::SteadyTimestamp(time_point);
  }

  opentelemetry::common::SystemTimestamp ToSystemNanoseconds(uint64_t timestamp_pico) const {
    const std::chrono::nanoseconds nano_sec(time_offset_ / kPicoToNanoDenominator + timestamp_pico);
    const ts_system time_point(nano_sec);
    return opentelemetry::common::SystemTimestamp(time_point);
  }

  static void add_Event(std::map<std::string, std::string> &attributes, const std::shared_ptr<Event> &event) {
    assert(event and "event is not null");
    const std::string type = GetTypeStr(event);
    attributes.insert({"timestamp", std::to_string(event->GetTs())});
    attributes.insert({"parser_ident", std::to_string(event->GetParserIdent())});
    attributes.insert({"parser name", event->GetParserName()});
    attributes.insert({"type", type});
  }

  static void add_SimSendSync(std::map<std::string, std::string> &attributes, const std::shared_ptr<SimSendSync> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
  }

  static void add_SimProcInEvent(std::map<std::string, std::string> &attributes,
                          const std::shared_ptr<SimProcInEvent> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
  }

  static void add_HostInstr(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostInstr> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"pc", std::to_string(event->GetPc())});
  }

  static void add_HostCall(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostCall> &event) {
    assert(event and "event is not null");
    add_HostInstr(attributes, event);
    attributes.insert({"func", *(event->GetFunc())});
    attributes.insert({"comp", *(event->GetComp())});
  }

  static void add_HostMmioImRespPoW(std::map<std::string, std::string> &attributes,
                             const std::shared_ptr<HostMmioImRespPoW> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
  }

  static void add_HostIdOp(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostIdOp> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"id", std::to_string(event->GetId())});
  }

  static void add_HostMmioCR(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostMmioCR> &event) {
    assert(event and "event is not null");
    add_HostIdOp(attributes, event);
  }

  static void add_HostMmioCW(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostMmioCW> &event) {
    assert(event and "event is not null");
    add_HostIdOp(attributes, event);
  }

  static void add_HostAddrSizeOp(std::map<std::string, std::string> &attributes,
                          const std::shared_ptr<HostAddrSizeOp> &event) {
    assert(event and "event is not null");
    add_HostIdOp(attributes, event);
    attributes.insert({"addr", std::to_string(event->GetAddr())});
    attributes.insert({"size", std::to_string(event->GetSize())});
  }

  static void add_HostMmioOp(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostMmioOp> &event) {
    assert(event and "event is not null");
    add_HostAddrSizeOp(attributes, event);
    attributes.insert({"bar", std::to_string(event->GetBar())});
    attributes.insert({"offset", std::to_string(event->GetOffset())});
  }

  static void add_HostMmioR(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostMmioR> &event) {
    assert(event and "event is not null");
    add_HostMmioOp(attributes, event);
  }

  static void add_HostMmioW(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostMmioW> &event) {
    assert(event and "event is not null");
    add_HostMmioOp(attributes, event);
  }

  static void add_HostDmaC(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostDmaC> &event) {
    assert(event and "event is not null");
    add_HostIdOp(attributes, event);
  }

  static void add_HostDmaR(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostDmaR> &event) {
    assert(event and "event is not null");
    add_HostAddrSizeOp(attributes, event);
  }

  static void add_HostDmaW(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostDmaW> &event) {
    assert(event and "event is not null");
    add_HostAddrSizeOp(attributes, event);
  }

  static void add_HostMsiX(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostMsiX> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"vec", std::to_string(event->GetVec())});
  }

  static void add_HostConf(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostConf> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"dev", std::to_string(event->GetDev())});
    attributes.insert({"func", std::to_string(event->GetFunc())});
    attributes.insert({"reg", std::to_string(event->GetReg())});
    attributes.insert({"bytes", std::to_string(event->GetBytes())});
    attributes.insert({"data", std::to_string(event->GetBytes())});
    attributes.insert({"is_read", event->IsRead() ? "true" : "false"});
  }

  static void add_HostClearInt(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostClearInt> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
  }

  static void add_HostPostInt(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostPostInt> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
  }

  static void add_HostPciRW(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostPciRW> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"offset", std::to_string(event->GetOffset())});
    attributes.insert({"size", std::to_string(event->GetSize())});
    attributes.insert({"is_read", event->IsRead() ? "true" : "false"});
  }

  static void add_NicMsix(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicMsix> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"vec", std::to_string(event->GetVec())});
    attributes.insert({"isX", BoolToString(event->IsX())});
  }

  static void add_NicDma(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicDma> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"id", std::to_string(event->GetId())});
    attributes.insert({"addr", std::to_string(event->GetId())});
    attributes.insert({"len", std::to_string(event->GetLen())});
  }

  static void add_SetIX(std::map<std::string, std::string> &attributes, const std::shared_ptr<SetIX> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"intr", std::to_string(event->GetIntr())});
  }

  static void add_NicDmaI(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicDmaI> &event) {
    assert(event and "event is not null");
    add_NicDma(attributes, event);
  }

  static void add_NicDmaEx(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicDmaEx> &event) {
    assert(event and "event is not null");
    add_NicDma(attributes, event);
  }

  static void add_NicDmaEn(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicDmaEn> &event) {
    assert(event and "event is not null");
    add_NicDma(attributes, event);
  }

  static void add_NicDmaCR(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicDmaCR> &event) {
    assert(event and "event is not null");
    add_NicDma(attributes, event);
  }

  static void add_NicDmaCW(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicDmaCW> &event) {
    assert(event and "event is not null");
    add_NicDma(attributes, event);
  }

  static void add_NicMmio(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicMmio> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"off", std::to_string(event->GetOff())});
    attributes.insert({"len", std::to_string(event->GetLen())});
    attributes.insert({"val", std::to_string(event->GetVal())});
  }

  void add_NicMmioR(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicMmioR> &event) {
    assert(event and "event is not null");
    add_NicMmio(attributes, event);
  }

  void add_NicMmioW(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicMmioW> &event) {
    assert(event and "event is not null");
    add_NicMmio(attributes, event);
  }

  void add_NicTrx(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicTrx> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"len", std::to_string(event->GetLen())});
  }

  void add_NicTx(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicTx> &event) {
    assert(event and "event is not null");
    add_NicTrx(attributes, event);
  }

  void add_NicRx(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicRx> &event) {
    assert(event and "event is not null");
    add_NicTrx(attributes, event);
    attributes.insert({"port", std::to_string(event->GetPort())});
  }

  opentelemetry::trace::StartSpanOptions GetSpanStartOpts(const std::shared_ptr<EventSpan> &span) {
    opentelemetry::trace::StartSpanOptions span_options;
    if (span->HasParent()) {
      auto custom_context = span->GetContext();
      auto parent = custom_context->GetParent();
      auto parent_context = parent->GetContext();
      throw_if_empty(parent_context, "GetSpanStartOpts, parent context is null");
      auto open_context = GetContext(parent_context);
      span_options.parent = open_context;
    }
    span_options.start_system_time = ToSystemNanoseconds(span->GetStartingTs());
    span_options.start_steady_time = ToSteadyNanoseconds(span->GetStartingTs());

    return std::move(span_options);
  }

  void end_span(const std::shared_ptr<EventSpan> &old_span, span_t &new_span) {
    assert(old_span and "old span is null");
    assert(new_span and "new span is null");
    opentelemetry::trace::EndSpanOptions end_opts;
    end_opts.end_steady_time = ToSteadyNanoseconds(old_span->GetCompletionTs());
    new_span->End(end_opts);
    RemoveSpan(old_span);
  }

  static void set_EventSpanAttr(span_t &new_span, std::shared_ptr<EventSpan> old_span) {
    auto span_name = GetTypeStr(old_span);
    new_span->SetAttribute("id", std::to_string(old_span->GetId()));
    new_span->SetAttribute("source id", std::to_string(old_span->GetSourceId()));
    new_span->SetAttribute("type", span_name);
    new_span->SetAttribute("pending", BoolToString(old_span->IsPending()));
    auto context = old_span->GetContext();
    throw_if_empty(context, "add_EventSpanAttr context is null");
    new_span->SetAttribute("trace id", std::to_string(context->GetTraceId()));
    if (context->HasParent()) {
      auto parent = context->GetParent();
      new_span->SetAttribute("parent_id", parent->GetId());
    }
  }

  static void set_HostCallSpanAttr(span_t &new_span, std::shared_ptr<HostCallSpan> &old_span) {
    set_EventSpanAttr(new_span, old_span);
    new_span->SetAttribute("kernel-transmit", BoolToString(old_span->DoesKernelTransmit()));
    new_span->SetAttribute("driver-transmit", BoolToString(old_span->DoesDriverTransmit()));
    new_span->SetAttribute("kernel-receive", BoolToString(old_span->DoesKernelReceive()));
    new_span->SetAttribute("driver-receive", BoolToString(old_span->DoesDriverReceive()));
    new_span->SetAttribute("overall-transmit", BoolToString(old_span->IsOverallTx()));
    new_span->SetAttribute("overall-receive", BoolToString(old_span->IsOverallRx()));
    new_span->SetAttribute("fragmented", BoolToString(old_span->IsFragmented()));
    const bool is_copy = old_span->IsCopy();
    new_span->SetAttribute("is-copy", BoolToString(is_copy));
    if (is_copy) {
      new_span->SetAttribute("original-id", std::to_string(old_span->GetOriginalId()));
    }
  }

  static void set_HostDmaSpanAttr(span_t &new_span, std::shared_ptr<HostDmaSpan> &old_span) {
    set_EventSpanAttr(new_span, old_span);
    new_span->SetAttribute("is-read", BoolToString(old_span->IsRead()));
  }

  static void set_HostMmioSpanAttr(span_t &new_span, std::shared_ptr<HostMmioSpan> &old_span) {
    set_EventSpanAttr(new_span, old_span);
    new_span->SetAttribute("is-read", BoolToString(old_span->IsRead()));
    //new_span->SetAttribute("after-pci/pci-before", BoolToString(old_span->IsAfterPci()));
    new_span->SetAttribute("BAR-number", std::to_string(old_span->GetBarNumber()));
    new_span->SetAttribute("is-going-to-device",
                           BoolToString(TraceEnvironment::IsToDeviceBarNumber(old_span->GetBarNumber())));
  }

  static void set_HostPciSpanAttr(span_t &new_span, std::shared_ptr<HostPciSpan> &old_span) {
    set_EventSpanAttr(new_span, old_span);
    new_span->SetAttribute("is-read", BoolToString(old_span->IsRead()));
  }

  static void set_NicMmioSpanAttr(span_t &new_span, std::shared_ptr<NicMmioSpan> &old_span) {
    set_EventSpanAttr(new_span, old_span);
    new_span->SetAttribute("is-read", BoolToString(old_span->IsRead()));
  }

  static void set_NicDmaSpanAttr(span_t &new_span, std::shared_ptr<NicDmaSpan> &old_span) {
    set_EventSpanAttr(new_span, old_span);
    new_span->SetAttribute("is-read", BoolToString(old_span->IsRead()));
  }

  static void set_NicEthSpanAttr(span_t &new_span, std::shared_ptr<NicEthSpan> &old_span) {
    set_EventSpanAttr(new_span, old_span);
    new_span->SetAttribute("is-transmit", BoolToString(old_span->IsTransmit()));
  }

  static void set_Attr(span_t &span, std::shared_ptr<EventSpan> &to_end) {
    switch (to_end->GetType()) {
      case kHostCall: {
        auto call_span = std::static_pointer_cast<HostCallSpan>(to_end);
        set_HostCallSpanAttr(span, call_span);
        break;
      }
      case kHostMmio: {
        auto mmio_span = std::static_pointer_cast<HostMmioSpan>(to_end);
        set_HostMmioSpanAttr(span, mmio_span);
        break;
      }
      case kHostPci: {
        auto pci_span = std::static_pointer_cast<HostPciSpan>(to_end);
        set_HostPciSpanAttr(span, pci_span);
        break;
      }
      case kHostDma: {
        auto dma_span = std::static_pointer_cast<HostDmaSpan>(to_end);
        set_HostDmaSpanAttr(span, dma_span);
        break;
      }
      case kNicDma: {
        auto dma_span = std::static_pointer_cast<NicDmaSpan>(to_end);
        set_NicDmaSpanAttr(span, dma_span);
        break;
      }
      case kNicMmio: {
        auto mmio_span = std::static_pointer_cast<NicMmioSpan>(to_end);
        set_NicMmioSpanAttr(span, mmio_span);
        break;
      }
      case kNicEth: {
        auto eth_span = std::static_pointer_cast<NicEthSpan>(to_end);
        set_NicEthSpanAttr(span, eth_span);
        break;
      }
      case kNicMsix:
      case kGenericSingle:
      case kHostInt:
      case kHostMsix:
      default: {
        set_EventSpanAttr(span, to_end);
      }
    }
  }

  void add_Events(span_t &span, std::shared_ptr<EventSpan> &to_end) {
    const size_t amount_events = to_end->GetAmountEvents();
    for (size_t index = 0; index < amount_events; index++) {

      auto action = to_end->GetAt(index);
      throw_if_empty(action, "EndSpan action is null");

      auto type = GetTypeStr(action);
      std::map<std::string, std::string> attributes;

      switch (action->GetType()) {
        case EventType::kHostCallT: {
          add_HostCall(attributes, std::static_pointer_cast<HostCall>(action));
          break;
        }
        case EventType::kHostMsiXT: {
          add_HostMsiX(attributes, std::static_pointer_cast<HostMsiX>(action));
          break;
        }
        case EventType::kHostMmioWT: {
          add_HostMmioW(attributes, std::static_pointer_cast<HostMmioW>(action));
          break;
        }
        case EventType::kHostMmioRT: {
          add_HostMmioR(attributes, std::static_pointer_cast<HostMmioR>(action));
          break;
        }
        case EventType::kHostMmioImRespPoWT: {
          add_HostMmioImRespPoW(attributes,
                                std::static_pointer_cast<HostMmioImRespPoW>(action));
          break;
        }
        case EventType::kHostMmioCWT: {
          add_HostMmioCW(attributes, std::static_pointer_cast<HostMmioCW>(action));
          break;
        }
        case EventType::kHostMmioCRT: {
          add_HostMmioCR(attributes, std::static_pointer_cast<HostMmioCR>(action));
          break;
        }
        case EventType::kHostPciRWT: {
          add_HostPciRW(attributes, std::static_pointer_cast<HostPciRW>(action));
          break;
        }
        case EventType::kHostConfT: {
          add_HostConf(attributes, std::static_pointer_cast<HostConf>(action));
          break;
        }
        case EventType::kHostDmaWT: {
          add_HostDmaW(attributes, std::static_pointer_cast<HostDmaW>(action));
          break;
        }
        case EventType::kHostDmaRT: {
          add_HostDmaR(attributes, std::static_pointer_cast<HostDmaR>(action));
          break;
        }
        case EventType::kHostDmaCT: {
          add_HostDmaC(attributes, std::static_pointer_cast<HostDmaC>(action));
          break;
        }
        case EventType::kHostPostIntT: {
          add_HostPostInt(attributes, std::static_pointer_cast<HostPostInt>(action));
          break;
        }
        case EventType::kHostClearIntT: {
          add_HostClearInt(attributes, std::static_pointer_cast<HostClearInt>(action));
          break;
        }
        case EventType::kNicDmaIT: {
          add_NicDmaI(attributes, std::static_pointer_cast<NicDmaI>(action));
          break;
        }
        case EventType::kNicDmaExT: {
          add_NicDmaEx(attributes, std::static_pointer_cast<NicDmaEx>(action));
          break;
        }
        case EventType::kNicDmaCWT: {
          add_NicDmaCW(attributes, std::static_pointer_cast<NicDmaCW>(action));
          break;
        }
        case EventType::kNicDmaCRT: {
          add_NicDmaCR(attributes, std::static_pointer_cast<NicDmaCR>(action));
          break;
        }
        case EventType::kNicMmioRT: {
          add_NicMmioR(attributes, std::static_pointer_cast<NicMmioR>(action));
          break;
        }
        case EventType::kNicMmioWT: {
          add_NicMmioW(attributes, std::static_pointer_cast<NicMmioW>(action));
          break;
        }
        case EventType::kNicRxT: {
          add_NicRx(attributes, std::static_pointer_cast<NicRx>(action));
          break;
        }
        case EventType::kNicTxT: {
          add_NicTx(attributes, std::static_pointer_cast<NicTx>(action));
          break;
        }
        case EventType::kNicMsixT: {
          add_NicMsix(attributes, std::static_pointer_cast<NicMsix>(action));
          break;
        }
        default: {
          throw_just("transform_HostCallSpan unexpected event: ", *action);
        }
      }

      span->AddEvent(type, ToSystemNanoseconds(action->GetTs()), attributes);
    }
  }

 public:
  explicit OtlpSpanExporter(const std::string &&url, bool batch_mode, std::string &&lib_name)
      : time_offset_(GetNowOffsetMicroseconds()), url_(url), batch_mode_(batch_mode), lib_name_(lib_name) {
  }

  explicit OtlpSpanExporter(const std::string &url, bool batch_mode, std::string &&lib_name)
      : time_offset_(GetNowOffsetMicroseconds()), url_(url), batch_mode_(batch_mode), lib_name_(lib_name) {
  }

  ~OtlpSpanExporter() {
    auto provider = opentelemetry::trace::Provider::GetTracerProvider();
    if (provider) {
      static_cast<opentelemetry::sdk::trace::TracerProvider *>(provider.get())->ForceFlush();
    }
    const std::shared_ptr<opentelemetry::trace::TracerProvider> none;
    opentelemetry::trace::Provider::SetTracerProvider(none);
  }

  void StartSpan(std::shared_ptr<EventSpan> to_start) override {
    const std::lock_guard<std::recursive_mutex> guard(exporter_mutex_);
    auto span_opts = GetSpanStartOpts(to_start);
    auto span_name = GetTypeStr(to_start);
    auto tracer = GetTracerLazy(to_start->GetServiceName());
    auto span = tracer->StartSpan(span_name, span_opts);
    InsertNewSpan(to_start, span);

    auto old_context = to_start->GetContext();
    auto new_context = span->GetContext();
    InsertNewContext(old_context, new_context);
  }

  void EndSpan(std::shared_ptr<EventSpan> to_end) override {
    const std::lock_guard<std::recursive_mutex> guard(exporter_mutex_);
    span_t span = GetSpan(to_end);
    set_Attr(span, to_end);
    add_Events(span, to_end);
    end_span(to_end, span);
  }

  void ExportSpan(std::shared_ptr<EventSpan> to_export) override {
    const std::lock_guard<std::recursive_mutex> guard(exporter_mutex_);
    StartSpan(to_export);
    EndSpan(to_export);
  }
};

}  // namespace simbricks::trace

#endif //SIMBRICKS_TRACE_EXPORTER_H_
