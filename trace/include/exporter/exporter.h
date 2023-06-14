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
#include "analytics/span.h"

#ifndef SIMBRICKS_TRACE_EXPORTER_H_
#define SIMBRICKS_TRACE_EXPORTER_H_

namespace simbricks::trace {

class OtlpSpanExporter {

  int64_t time_offset_ = 0;
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> tracer_;
  std::unordered_map<uint64_t, opentelemetry::trace::SpanContext> contex_map_;

  using ts_steady = std::chrono::time_point<std::chrono::steady_clock, std::chrono::microseconds>;
  using ts_system = std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds>;

  void update_context_map(std::shared_ptr<EventSpan> old_span,
                          opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>& new_span) {
    auto context = new_span->GetContext();
    const bool updated = contex_map_.insert({old_span->get_id(), context}).second;
    throw_on(not updated, "could not update context map");
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
    sss << span->get_type();
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

  int64_t get_now_offset_microseconds() {
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::microseconds>(now);
    auto value = now_ms.time_since_epoch();
    return value.count();
  }

  opentelemetry::common::SteadyTimestamp to_steady_microseconds(uint64_t timestamp) {
    const ts_steady time_point{std::chrono::microseconds{time_offset_ + (timestamp / 1000 / 1000)}};
    return opentelemetry::common::SteadyTimestamp(time_point);
  }

  opentelemetry::common::SystemTimestamp to_system_microseconds(uint64_t timestamp) {
    const ts_system time_point{std::chrono::microseconds{time_offset_ + (timestamp / 1000 / 1000)}};
    return opentelemetry::common::SystemTimestamp(time_point);
  }

  opentelemetry::trace::StartSpanOptions get_span_start_opts(std::shared_ptr<EventSpan> span) {
    opentelemetry::trace::StartSpanOptions span_options;
    if (span->has_parent()) {
      auto parent = span->get_parent();
      throw_if_empty(parent, event_is_null);
      const uint64_t parent_id = parent->get_id();

      auto search = contex_map_.find(parent_id);
      throw_on(search == contex_map_.end(), "the parent has not yet an entry within the context map");

      span_options.parent = search->second;
    }
    span_options.start_system_time = to_system_microseconds(span->get_starting_ts());
    span_options.start_steady_time = to_steady_microseconds(span->get_starting_ts());
    
    return std::move(span_options);
  }

  void end_span(std::shared_ptr<EventSpan> old_span,
                opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>& new_span) {
    assert(old_span and "old span is null");
    assert(new_span and "new span is null");
    opentelemetry::trace::EndSpanOptions end_opts;
    end_opts.end_steady_time = to_steady_microseconds(old_span->get_completion_ts());
    new_span->End(end_opts);
    update_context_map(old_span, new_span);
  }

  void add_HostMmioW(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>& span, std::shared_ptr<HostMmioW> event) {
    const std::string type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_HostMmioW(attributes, event);
    span->AddEvent(type, to_system_microseconds(event->get_ts()), attributes);
  }

  void add_HostMmioR(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>& span, std::shared_ptr<HostMmioR> event) {
    const std::string type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_HostMmioR(attributes, event);
    span->AddEvent(type, to_system_microseconds(event->get_ts()), attributes);
  }

  void add_HostMmioImRespPoW(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>& span, std::shared_ptr<HostMmioImRespPoW> event) {
    const std::string type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_HostMmioImRespPoW(attributes, event);
    span->AddEvent(type, to_system_microseconds(event->get_ts()), attributes);
  }

  void add_HostMmioCW(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>& span, std::shared_ptr<HostMmioCW> event) {
    const std::string type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_HostMmioCW(attributes, event);
    span->AddEvent(type, to_system_microseconds(event->get_ts()), attributes);
  }

  void add_HostMmioCR(opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>& span, std::shared_ptr<HostMmioCR> event) {
    const std::string type = get_type_str(event);
    std::map<std::string, std::string> attributes;
    add_HostMmioCR(attributes, event);
    span->AddEvent(type, to_system_microseconds(event->get_ts()), attributes);
  }

  void transform_HostCallSpan(std::shared_ptr<HostCallSpan> &to_transform) {
    // TODO
  }

  void transform_HostMsixSpan(std::shared_ptr<HostMsixSpan> &to_transform) {
    // TODO
  }

  void transform_HostMmioSpan(std::shared_ptr<HostMmioSpan> &to_transform) {
    auto span_options = get_span_start_opts(to_transform);
    std::stringstream span_name;
    span_name << to_transform->get_type();
    const std::map<std::string, std::string> span_attributes {
        {"id", std::to_string(to_transform->get_id())},
        {"source id", std::to_string(to_transform->get_source_id())},
        {"type", span_name.str()},
        {"pending", to_transform->is_pending() ? "true" : "false"},
        {"trace id", std::to_string(to_transform->get_trace_id())}
    };
    auto mmio_span = tracer_->StartSpan(span_name.str(), span_attributes, span_options);

    for (auto &event : to_transform->events_) {
      switch (event->get_type()) {
        case EventType::HostMmioW_t:
          add_HostMmioW(mmio_span, std::static_pointer_cast<HostMmioW>(event));
          break;
        case EventType::HostMmioR_t:
          add_HostMmioR(mmio_span, std::static_pointer_cast<HostMmioR>(event));
          break;

        case EventType::HostMmioImRespPoW_t:
          add_HostMmioImRespPoW(mmio_span, std::static_pointer_cast<HostMmioImRespPoW>(event));
          break;

        case EventType::HostMmioCW_t:
          add_HostMmioCW(mmio_span, std::static_pointer_cast<HostMmioCW>(event));
          break;
        case EventType::HostMmioCR_t:
          add_HostMmioCR(mmio_span, std::static_pointer_cast<HostMmioCR>(event));
          break;

        default:
          throw_just("transform_HostMmioSpan unexpected event: ", *event);
      }
    }

    end_span(to_transform, mmio_span);
  }


  void transform_HostDmaSpan(std::shared_ptr<HostDmaSpan> &to_transform) {
    // TODO
  }

  void transform_HostIntSpan(std::shared_ptr<HostIntSpan> &to_transform) {
    // TODO
  }

  void transform_NicDmaSpan(std::shared_ptr<NicDmaSpan> &to_transform) {
    // TODO
  }

  void transform_NicMmioSpan(std::shared_ptr<NicMmioSpan> &to_transform) {
    // TODO
  }

  void transform_NicEthSpan(std::shared_ptr<NicEthSpan> &to_transform) {
    // TODO
  }

  void transform_NicMsixSpan(std::shared_ptr<NicMsixSpan> &to_transform) {
    // TODO
  }

  void transform_GenericSingleSpan(std::shared_ptr<GenericSingleSpan> &to_transform) {
    // TODO
  }


  public:
  OtlpSpanExporter(std::string url, std::string service_name, bool batch_mode, std::string lib_name) : SpanExporter() {
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
    time_offset_ = get_now_offset_microseconds();
  }

  ~OtlpSpanExporter() {
    auto provider = opentelemetry::trace::Provider::GetTracerProvider();
    if (provider)
    {
      static_cast<opentelemetry::sdk::trace::TracerProvider *>(provider.get())->ForceFlush();
    }
    const std::shared_ptr<opentelemetry::trace::TracerProvider> none;
    opentelemetry::trace::Provider::SetTracerProvider(none);
  }

  void export_span(std::shared_ptr<EventSpan> &span_to_export) {
    return;
  }
};

}  // namespace simbricks::trace

#endif //SIMBRICKS_TRACE_EXPORTER_H_
