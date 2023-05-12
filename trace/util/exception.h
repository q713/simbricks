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

#include <stdexcept>
#include <memory>

inline const char *resume_executor_null =
        "std::shared_ptr<concurrencpp::executor> is null";
inline const char *channel_is_null = "std::shared_ptr<channel<ValueType>> is null";
inline const char *pipe_is_null = "std::shared_ptr<pipe<ValueType>> is null";
inline const char *consumer_is_null = "std::shared_ptr<consumer<ValueType>> is null";
inline const char *producer_is_null = "std::shared_ptr<producer<ValueType>> is null";
inline const char *event_is_null = "std::shared_ptr<Event> is null";
inline const char *trace_is_null = "std::shared_ptr<Trace> is null";
inline const char *span_is_null = "std::shared_ptr<Span> is null";
inline const char *parser_is_null = "std::shared_ptr<LogParser> is null";
inline const char *actor_is_null = "std::shared_ptr<event_stream_actor> is null";
inline const char *printer_is_null = "a printer is null";

template<typename Value>
void throw_if_empty (std::shared_ptr<Value> to_check, const char *message)
{
  if (not to_check)
  {
    throw std::runtime_error (message);
  }
}

template<typename Value>
void throw_if_empty (Value *to_check, const char *message)
{
  if (not to_check)
  {
    throw std::runtime_error (message);
  }
}

#endif //SIMBRICKS_TRACE_EXCEPTION_H_
