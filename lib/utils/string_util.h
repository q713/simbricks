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

#ifndef SIMBRICKS_STRING_UTILS_H_
#define SIMBRICKS_STRING_UTILS_H_

#include <algorithm>
#include <functional>
#include <string>
#include <sstream>

namespace sim_string_utils {

/*
 * This function will copy the the contents of
 * to_copy into target. For that it will allocate memory
 * on the heap and assign target to that location and
 * afterwards copy the contents of to:copy to that memory region.
 *
 * Note: the caller must ensre freeing the for target allocated memory.
 */
bool copy_and_assign_terminate(const char *&target,
                               const std::string &to_copy) {
  std::size_t length = to_copy.length();
  char *tmp = new char[length + 1];
  if (tmp == nullptr)
    return false;

  if (to_copy.copy(tmp, length)) {
    if (tmp[length] != '\0') {
      tmp[length] = '\0';
    }
    target = tmp;
    return true;
  }

  delete[] tmp;
  return false;
}

static std::function<bool(unsigned char)> is_space = [] (unsigned char c) {
  return std::isspace(c);
};

static std::function<bool(unsigned char)> is_alnum = [] (unsigned char c) {
        return std::isalnum(c) != 0;
};

/*
 * Trim all whitespaces from left to the first non whitespace character
 * of a string.
 */
inline void trimL(std::string &to_trim) {
  auto till = std::find_if_not(to_trim.begin(), to_trim.end(), is_space);
  if (till != to_trim.end())
    to_trim.erase(to_trim.begin(), till);
}

/*
 * Trim all whitespaces from right to the first non whitespace character 
 * of a string.
 */
inline void trimR(std::string &to_trim) {
  auto from = std::find_if_not(to_trim.rbegin(), to_trim.rend(), is_space).base(); 
  if (from != to_trim.end())
    to_trim.erase(from, to_trim.end());
}

/*
 * Trim all whitespaces from left to the first non whitespace character 
 * of a string and then trim all whitespaces from right to the first non
 * whitespace character of a string.
 */
inline void trim(std::string &to_trim) {
  trimL(to_trim);
  trimR(to_trim);
}

/*
 * Trim all non whitespaces from left to the first whitespace character 
 * of a string.
 */
inline void trimTillWhitespace(std::string &to_trim) {
  auto till = std::find_if(to_trim.begin(), to_trim.end(), is_space);
  if (till != to_trim.end())
    to_trim.erase(to_trim.begin(), till);
}

std::string extract_and_substr_until(std::string &extract_from, std::function<bool (unsigned char)> &predicate) {
  std::stringstream extract_builder;
  while (extract_from.length() != 0)
  {
    unsigned char letter = extract_from[0];
    if (!predicate(letter)) {
      break;
    }
    extract_builder << letter;
    extract_from = extract_from.substr(1);
  }
  return extract_builder.str();
}

bool consume_and_trim_string(std::string &find_and_trim, const std::string &to_consume) {
  auto res = find_and_trim.find(to_consume);
  if (res != 0 || res == std::string::npos) {
    return false;
  }
  auto begin = find_and_trim.begin();
  find_and_trim.erase(begin, begin + to_consume.length());
  return true;
}

bool consume_and_trim_char(std::string &find_and_trim, const char to_consume) {
  if (find_and_trim.empty()) {
    return false;
  }
  unsigned char letter = find_and_trim[0];
  if (letter != to_consume)
    return false;
  
  find_and_trim.erase(0, 1);
  return true;
}

}  // namespace sim_string_utils

#endif  // SIMBRICKS_STRING_UTILS_H_