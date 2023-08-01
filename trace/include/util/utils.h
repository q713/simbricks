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

#include <string>
#include <chrono>

#ifndef SIMBRICKS_TRACE_UTILS_H_
#define SIMBRICKS_TRACE_UTILS_H_

inline int64_t GetNowOffsetMicroseconds() {
  auto now = std::chrono::system_clock::now();
  auto now_ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(now);
  auto value = now_ns.time_since_epoch();
  return value.count();
}

inline std::string BoolToString(bool boo) {
  return boo ? "true" : "false";
}

inline void WriteIdent(std::ostream &out, unsigned ident) {
  if (ident == 0)
    return;

  for (size_t i = 0; i < ident; i++) {
    out << "\t";
  }
}

template<typename ObjT>
inline std::ostream &operator<<(std::ostream &out, std::shared_ptr<ObjT> &to_write) {
  if (to_write) {
    out << *to_write;
  } else {
    out << "null";
  }
  return out;
}

#endif //SIMBRICKS_TRACE_UTILS_H_
