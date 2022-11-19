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