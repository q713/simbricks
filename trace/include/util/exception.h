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

#ifndef SIMBRICKS_TRACE_EXCEPTION_H_
#define SIMBRICKS_TRACE_EXCEPTION_H_

#include <exception>
#include <stdexcept>
#include <memory>
#include <string>
#include <sstream>
#include <optional>
#include <iostream>
#include <source_location>

class TraceException : public std::exception {
  const std::string error_message_;

  static std::string build_error_msg(const std::string_view location,
                                     const std::string_view message) noexcept {
    std::stringstream err_msg{"TraceException"};
    err_msg << "occured in " << location << ": " << message;
    return err_msg.str();
  }

 public:
  static constexpr const char *kResumeExecutorNull = "concurrencpp::executor is null";
  static constexpr const char *kChannelIsNull = "channel<ValueType> is null";
  static constexpr const char *kPipeIsNull = "pipe<ValueType> is null";
  static constexpr const char *kConsumerIsNull = "consumer<ValueType> is null";
  static constexpr const char *kProducerIsNull = "producer<ValueType> is null";
  static constexpr const char *kEventIsNull = "Event is null";
  static constexpr const char *kTraceIsNull = "Trace is null";
  static constexpr const char *kSpanIsNull = "Span is null";
  static constexpr const char *kParserIsNull = "LogParser is null";
  static constexpr const char *kActorIsNull = "EventStreamActor is null";
  static constexpr const char *kPrinterIsNull = "printer is null";
  static constexpr const char *kContextIsNull = "context is null";
  static constexpr const char *kEventStreamParserNull = "EventStreamParser is null";
  static constexpr const char *kSpannerIsNull = "Spanner is null";
  static constexpr const char *kCouldNotPushToContextQueue = "could not push value into context queue";
  static constexpr const char *kQueueIsNull = "ContextQueue<...> is null";
  static constexpr const char *kSpanExporterNull = "SpanExporter is null";
  static constexpr const char *kSpanProcessorNull = "SpanProcessor is null";
  static constexpr const char *kTraceProviderNull = "TracerProvider is null";

  explicit TraceException(const char *location, const char *message)
      : error_message_(build_error_msg(location, message)) {}

  explicit TraceException(const std::string &location, const std::string &message) : error_message_(
      build_error_msg(location, message)) {}

  explicit TraceException(const std::string &&location, const std::string &&message) : error_message_(
      build_error_msg(location, message)) {}

  const char *what() const noexcept override {
    return error_message_.c_str();
  }
};

inline std::string LocationToString(const std::source_location &location) {
  std::stringstream msg{location.file_name()};
  msg << ":" << location.function_name();
  return msg.str();
}

template<typename Value>
inline void throw_if_empty(const std::shared_ptr<Value> &to_check,
                           const char *message,
                           const std::source_location &location = std::source_location::current()) {
  if (not to_check) {
    std::cout << "exception thrown" << '\n';
    std::cout << message << '\n';
    //std::terminate();
    throw TraceException(LocationToString(location), message);
  }
}

template<typename Value>
inline void throw_if_empty(const std::shared_ptr<Value> &to_check, std::string &&message) {
  throw_if_empty(to_check, message.c_str());
}

template<typename Value>
inline void throw_if_empty(const std::unique_ptr<Value> &to_check,
                           const char *message,
                           const std::source_location &location = std::source_location::current()) {
  if (not to_check) {
    std::cout << "exception thrown" << '\n';
    std::cout << message << '\n';
    //std::terminate();
    throw TraceException(LocationToString(location), message);
  }
}

template<typename Value>
inline void throw_if_empty(const Value *to_check,
                           const char *message,
                           const std::source_location &location = std::source_location::current()) {
  if (not to_check) {
    std::cout << "exception thrown" << '\n';
    std::cout << message << '\n';
    //std::terminate();
    throw TraceException(LocationToString(location), message);
  }
}

template<typename Value>
inline void throw_if_empty(const Value *to_check, std::string &&message) {
  throw_if_empty(to_check, message.c_str());
}

inline void throw_on(bool should_throw,
                     const char *message,
                     const std::source_location &location = std::source_location::current()) {
  if (should_throw) {
    std::cout << "exception thrown" << '\n';
    std::cout << message << '\n';
    //std::terminate();
    throw TraceException(LocationToString(location), message);
  }
}

inline void throw_on(bool should_throw, std::string &&message) {
  throw_on(should_throw, message.c_str());
}

template<typename ValueType>
ValueType OrElseThrow(std::optional<ValueType> &val_opt, const char *message) {
  throw_on(not val_opt.has_value(), message);
  return val_opt.value();
}

template<typename ValueType>
ValueType OrElseThrow(std::optional<ValueType> &val_opt, std::string &&message) {
  return OrElseThrow(val_opt, message.c_str());
}

template<typename ValueType>
ValueType OrElseThrow(std::optional<ValueType> &&val_opt, std::string &&message) {
  return OrElseThrow(val_opt, message.c_str());
}

template<typename ...Args>
inline void throw_just(const std::source_location &location, Args &&... args) {
  std::stringstream message_builder;
  ([&] {
    message_builder << args;
  }(), ...);
  std::cout << "exception thrown" << '\n';
  const std::string message{message_builder.str()};
  std::cout << message << '\n';
//std::terminate();
  throw TraceException(LocationToString(location), message);
}

#endif //SIMBRICKS_TRACE_EXCEPTION_H_
