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

#include <string>

namespace sim_string_utils {

/*
 * This function will copy the the contents of 
 * to_copy into target. For that it will allocate memory 
 * on the heap and assign target to that location and
 * afterwards copy the contents of to:copy to that memory region.
 * 
 * Note: the caller must ensre freeing the for target allocated memory.
 */
bool copy_and_assign_terminate(const char *&target, const std::string &to_copy) {
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

    delete [] tmp;
    return false;
}

} // namespace sim_string_utils

#endif // SIMBRICKS_STRING_UTILS_H_