#include <iostream>
#include <memory>
#include <string>

#include "opentelemetry/sdk/version/version.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_options.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h"
#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/trace/scope.h"
#include "opentelemetry/common/timestamp.h"

#include "exporter/exporter.h"
#include "events/events.h"
#include "analytics/span.h"


int main() {

  const simbricks::trace::OtlpSpanExporter exporter("http://localhost:4318/v1/traces",
                                     "dummy-simbricks-tracer", true, "dummy application");

  auto tracer = exporter.get_tracer();
  auto offset = exporter.get_offset();

  // syscall span starting the trace
  opentelemetry::trace::StartSpanOptions start_opts;
  start_opts.start_steady_time = simbricks::trace::SpanExporter::to_steady_microseconds(1967468831374, offset);
  start_opts.start_system_time = simbricks::trace::SpanExporter::to_system_microseconds(1967468831374, offset);
  auto call_span = tracer->StartSpan("syscall span", start_opts);
  auto scoped_span_lib = opentelemetry::trace::Scope(call_span);
  call_span->AddEvent("lan_xmit_send", simbricks::trace::SpanExporter::to_system_microseconds(1967468831374, offset));

  // mmio span
  auto mmio_w = std::make_shared<HostMmioW>(1967468841374, 1,
                                            "test parser", 94469376773312, 108000, 4, 0, 0);
  auto mmio_imr = std::make_shared<HostMmioImRespPoW>(1967468841374, 1, "test parser");
  auto mmio_cw = std::make_shared<HostMmioCW>(1967469841374, 1, "test parser", 94469376773312);
  HostMmioSpan span{1, false};
  span.add_to_span(mmio_w);
  span.add_to_span(mmio_imr);
  span.add_to_span(mmio_cw);


  opentelemetry::trace::StartSpanOptions mmio_opts;
  mmio_opts.parent = call_span->GetContext();
  mmio_opts.start_system_time = simbricks::trace::SpanExporter::to_system_microseconds(1967468841374, offset);
  mmio_opts.start_steady_time = simbricks::trace::SpanExporter::to_steady_microseconds(1967468841374, offset);
  auto mmio_span = tracer->StartSpan("mmio write span", mmio_opts);
  auto scoped_span_f2 = opentelemetry::trace::Scope(mmio_span);
  const std::map<std::string, std::string> mmio_w_attributes {
      {"timestamp", "1967468841374"},
      {"parser_ident", "1"},
      {"parser name", "test parser"},
      {"id", "94469376773312"},
      {"address", "108000"},
      {"size", "4"},
      {"bar", "0"},
      {"offset", "0"}
  };
  mmio_span->AddEvent("HostMmioW", simbricks::trace::SpanExporter::to_system_microseconds(1967468841374, offset), mmio_w_attributes);
  mmio_span->SetAttribute("service.name", "Mmio Spanner");
  mmio_span->AddEvent("HostMmioImResponse", simbricks::trace::SpanExporter::to_system_microseconds(1967468841374, offset));
  mmio_span->AddEvent("HostMmioCW", simbricks::trace::SpanExporter::to_system_microseconds(1967469841374, offset));
  opentelemetry::trace::EndSpanOptions end_mmio;
  end_mmio.end_steady_time = simbricks::trace::SpanExporter::to_steady_microseconds(1967469841374, offset);
  mmio_span->End(end_mmio);


  // end call span
  opentelemetry::trace::EndSpanOptions end_call;
  end_call.end_steady_time = simbricks::trace::SpanExporter::to_steady_microseconds(1967469891374, offset);
  call_span->AddEvent("return via sysret", simbricks::trace::SpanExporter::to_system_microseconds(1967469891374, offset));
  call_span->End(end_call);

}
