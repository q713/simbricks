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

#include "util/concepts.h"
#include "util/exception.h"
#include "util/string_util.h"

#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <utility>
#include <fcntl.h>

#ifndef SIMBRICKS_TRACE_CREADER_H_
#define SIMBRICKS_TRACE_CREADER_H_

//class CLineHandler {
class LineHandler {
  char *buf_;
  size_t size_;
  size_t cur_reading_pos_ = 0;

 public:
  //explicit CLineHandler(char *buf, size_t size) : buf_(buf), size_(size) {
  explicit LineHandler(char *buf, size_t size) : buf_(buf), size_(size) {
    Reset(buf, size_);
  };

  void Reset(char *buf, size_t size) {
    throw_if_empty(buf_, "nullpointer to current line", source_loc::current());
    buf_ = buf;
    size_ = size;
    cur_reading_pos_ = 0;
  }

  inline std::string GetRawLine() {
    return {buf_, buf_ + size_};
  }

  inline void ResetPos() {
    cur_reading_pos_ = 0;
  }

  inline std::string GetCurString() {
    if (cur_reading_pos_ >= size_) {
      return "";
    }
    return {buf_ + cur_reading_pos_, buf_ + size_};
  }

  [[nodiscard]] inline size_t CurLength() const {
    return size_ - cur_reading_pos_;
  }

  [[nodiscard]] inline bool IsEmpty() const {
    return CurLength() <= 0;
  }

  bool MoveForward(size_t steps) {
    if (IsEmpty() || CurLength() < steps)
      return false;

    cur_reading_pos_ += steps;
    return true;
  }

  void TrimL() {
    if (IsEmpty())
      return;

    char *tmp = buf_ + cur_reading_pos_;
    for (; tmp < buf_ + size_; tmp++) {
      if (sim_string_utils::is_space(*tmp)) {
        ++cur_reading_pos_;
      } else {
        break;
      }
    }
  }

  void TrimTillWhitespace() {
    if (IsEmpty())
      return;

    char *tmp = buf_ + cur_reading_pos_;
    for (; tmp < buf_ + size_; tmp++) {
      if (not sim_string_utils::is_space(*tmp)) {
        ++cur_reading_pos_;
      } else {
        break;
      }
    }
  }

  std::string ExtractAndSubstrUntil(
      std::function<bool(unsigned char)> &predicate) {
    std::stringstream extract_builder;
    while (!IsEmpty()) {
      unsigned char letter = *(buf_ + cur_reading_pos_);
      if (!predicate(letter)) {
        break;
      }
      extract_builder << letter;
      ++cur_reading_pos_;
    }
    return extract_builder.str();
  }

  bool ExtractAndSubstrUntilInto(
      std::string &target, std::function<bool(unsigned char)> &predicate) {
    target = ExtractAndSubstrUntil(predicate);
    return not target.empty();
  }

  bool SkipTill(std::function<bool(unsigned char)> &predicate) {
    if (IsEmpty()) {
      return false;
    }

    size_t read = 0;
    char *tmp = buf_ + cur_reading_pos_;
    for (; tmp < buf_ + size_; tmp++) {
      if (predicate(*tmp)) {
        break;
      } else {
        read++;
      }
    }

    if (buf_ + cur_reading_pos_ + read < buf_ + size_) {
      cur_reading_pos_ += read;
      return true;
    }

    return false;
  }

  bool SkipTillWhitespace() {
    return SkipTill(sim_string_utils::is_space);
  }

  bool ConsumeAndTrimTillString(const std::string &to_consume) {
    if (IsEmpty() || CurLength() < to_consume.length()) {
      return false;
    }

    char *sub_start = buf_ + cur_reading_pos_;
    char *sub_end = buf_ + size_;
    auto tf_start = to_consume.begin();
    auto tf_end = to_consume.end();

    size_t consumed = 0;
    const size_t to_match = to_consume.length();
    int matched;
    while (sub_start != sub_end and tf_start != tf_end) {
      // search for potential start
      for (; *tf_start != *sub_start and sub_start != sub_end; sub_start++, consumed++);
      if (sub_start == sub_end) {
        return false;
      }

      // try matching
      matched = 0;
      for (; tf_start != tf_end; ++tf_start, ++sub_start, ++consumed) {
        if (sub_start == sub_end) {
          return false;
        }

        const char trie = *tf_start;
        const char match = *sub_start;
        if (trie != match) {
          break;
        }
        ++matched;
      }

      if (matched == to_match) {
        cur_reading_pos_ = cur_reading_pos_ + consumed;
        return true;
      }
      tf_start -= matched;
    }

    return false;
  }

  bool ConsumeAndTrimString(const std::string &to_consume) {
    if (IsEmpty() || CurLength() < to_consume.length()) {
      return false;
    }

    char *sub_start = buf_ + cur_reading_pos_;
    auto tf_start = to_consume.begin();
    auto tf_end = to_consume.end();

    size_t consumed = 0;
    while (tf_start != tf_end) {
      if (sub_start == buf_ + size_) {
        return false;
      }
      const char to_match = *tf_start;
      const char match = *sub_start;
      if (to_match != match) {
        return false;
      }
      ++sub_start;
      ++tf_start;
      ++consumed;
    }

    cur_reading_pos_ = cur_reading_pos_ + consumed;
    return true;
  }

  bool ConsumeAndTrimChar(char to_consume) {
    if (IsEmpty()) {
      return false;
    }
    const char letter = buf_[cur_reading_pos_];
    if (letter != to_consume)
      return false;

    ++cur_reading_pos_;
    return true;
  }

  bool ParseUintTrim(int base, uint64_t &target) {
    if (IsEmpty() or (base != 10 and base != 16)) {
      return false;
    }
    auto pred =
        base == 10 ? sim_string_utils::is_num : sim_string_utils::is_alnum;

    const size_t old_reading_pos = cur_reading_pos_;
    size_t length = 0;
    while (!IsEmpty()) {
      unsigned char letter = *(buf_ + old_reading_pos + length);
      if (!pred(letter)) {
        break;
      }
      ++length;
    }
    if (length == 0 or old_reading_pos + length > size_) {
      cur_reading_pos_ = old_reading_pos;
      return false;
    }

    char *end;
    const uint64_t num = std::strtoul(buf_ + old_reading_pos, &end, base);
    if (num == ULONG_MAX) {
      cur_reading_pos_ = old_reading_pos;
      return false;
    }

    target = num;
    cur_reading_pos_ += length;
    return true;
  }

  bool ParseInt(int &target) {
    if (IsEmpty()) {
      return false;
    }

    const size_t old_reading_pos = cur_reading_pos_;
    size_t length = 0;
    while (!IsEmpty()) {
      unsigned char letter = *(buf_ + old_reading_pos + length);
      if (!sim_string_utils::is_num(letter)) {
        break;
      }
      ++length;
    }
    if (length == 0 or old_reading_pos + length > size_) {
      cur_reading_pos_ = old_reading_pos;
      return false;
    }

    char *end;
    target = std::strtol(buf_ + old_reading_pos, &end, 10);
    cur_reading_pos_ += length;
    return true;
  }

  bool ParseBoolFromUint(int base, bool &target) {
    uint64_t internal_target;
    if (not ParseUintTrim(base, internal_target)) {
      return false;
    }

    target = internal_target != 0;
    return true;
  }

  bool ParseBoolFromStringRepr(bool &target) {
    if (ConsumeAndTrimString("true")) {
      target = true;
      return true;
    }
    if (ConsumeAndTrimString("false")) {
      target = false;
      return true;
    }
    return false;
  }

  bool ParseBoolFromInt(bool &target) {
    int value;
    if (not ParseInt(value)) {
      return false;
    }
    target = value != 0;
    return true;
  }
};

template<size_t BlockSize = 4 * 1024> requires SizeLagerZero<BlockSize>
//class CFileLineBufferReader {
class ReaderBuffer {

  const std::string name_;

  std::string cur_file_path_;
  FILE *file_ = nullptr;
  static constexpr char kLineEnd = '\n';
  char buffer_[BlockSize]{};
  size_t cur_reading_pos_ = 0;
  size_t size_ = 0;
  size_t next_line_end_ = 0;
  int reached_eof_ = 2;
  //CLineHandler line_handler_{buffer_, 0};
  LineHandler line_handler_{buffer_, 0};

  [[nodiscard]] bool IsStreamStillGood() {
    int file_descriptor = open(cur_file_path_.c_str(), O_RDONLY | O_NONBLOCK);
    close(file_descriptor);
    return file_descriptor != -1;
  }

  void NextBlock() {
    assert(IsStreamStillGood());
    if (reached_eof_ >= 2) {
      return;
    }

    if (cur_reading_pos_ != size_) {
      memmove(buffer_, buffer_ + cur_reading_pos_, size_ - cur_reading_pos_);
    }

    size_t next_start = size_ - cur_reading_pos_;
    size_t amount_to_read = BlockSize - next_start;
    assert(next_start + amount_to_read <= BlockSize);

    size_t actually_read = std::fread(buffer_ + next_start, sizeof(buffer_[0]), amount_to_read, file_);
    throw_on((actually_read == 0 and not std::feof(file_)) or std::ferror(file_),
             "error reading next block",
             source_loc::current());

    size_ = next_start + actually_read;
    assert(size_ <= BlockSize);
    cur_reading_pos_ = 0;
    next_line_end_ = 0;
  }

  void CalculateNextLineEnd() {
    if (next_line_end_ > 0) {
      return;
    }

    bool found_end = false;
    size_t tmp = cur_reading_pos_;
    for (; tmp < size_; tmp++) {
      if (buffer_[tmp] == kLineEnd) {
        found_end = true;
        break;
      }
    }

    if (found_end) {
      next_line_end_ = tmp;
    } else {
      if (std::feof(file_)) {
        next_line_end_ = size_;
        ++reached_eof_;
        return;
      }
      next_line_end_ = 0;
    }
  }

  [[nodiscard]] bool HasStillLineEnd() {
    CalculateNextLineEnd();
    return reached_eof_ < 2
        and size_ > 0
        and cur_reading_pos_ < size_
        and next_line_end_ > 0
        and cur_reading_pos_ < next_line_end_;
  }

  void Close() {
    if (file_) {
      fclose(file_);
      file_ = nullptr;
    }
  }

 public:
  //explicit CFileLineBufferReader(std::string name)
  explicit ReaderBuffer(std::string name)
      : name_(std::move(name)) {
  }

  //~CFileLineBufferReader() {
  ~ReaderBuffer() {
    Close();
  }

  [[nodiscard]] bool IsOpen() {
    return file_ != nullptr and IsStreamStillGood();
  }

  [[nodiscard]] bool HasStillLine() {
    while (cur_reading_pos_ < size_ and buffer_[cur_reading_pos_] == kLineEnd) {
      ++cur_reading_pos_;
      if (HasStillLineEnd()) {
        return true;
      }
    }

    if (HasStillLineEnd()) {
      return true;
    }
    if (not IsStreamStillGood()) {
      return false;
    }
    NextBlock();
    next_line_end_ = 0;

    return HasStillLineEnd();
  }

  //std::pair<bool, CLineHandler *> NextHandler() {
  std::pair<bool, LineHandler *> NextHandler() {
    if (not HasStillLine()) {
      return {false, nullptr};
    }

    line_handler_.Reset(buffer_ + cur_reading_pos_, next_line_end_ - cur_reading_pos_);
    cur_reading_pos_ = next_line_end_ + 1;
    next_line_end_ = 0;

    return {true, &line_handler_};
  }

  void OpenFile(const std::string &file_path, bool is_named_pipe = false) {
    cur_file_path_ = file_path;
    if (!std::filesystem::exists(file_path)) {
      throw_just(source_loc::current(),
                 "ReaderBuffer: the file path'", file_path, "' does not exist");
    }
    throw_on(file_, "ReaderBuffer:OpenFile: already opened file to read",
             source_loc::current());

    spdlog::debug("try open file path: ", file_path);
    file_ = fopen(file_path.c_str(), "r");
    throw_if_empty(file_, "ReaderBuffer: could not open file path", source_loc::current());
    reached_eof_ = 0;

    if (is_named_pipe) {
      const int file_descriptor = fileno(file_);
      throw_on(file_descriptor == -1, "ReaderBuffer: could not obtain fd", source_loc::current());
      const int suc = fcntl(file_descriptor, F_SETPIPE_SZ, BlockSize);
      if (suc != BlockSize) {
        spdlog::warn("ReaderBuffer: could not change '{}' size to {}", file_path, BlockSize);
      } else {
        spdlog::debug("ReaderBuffer: changed size successfully");
      }
    }
    spdlog::debug("successfully opened file path: ", file_path);
  }

};

#endif // SIMBRICKS_TRACE_CREADER_H_
