#include "handler_utils.h"

#include <cstring>

bool match_trimmed(const char* s, const char* expected) {
    if (s == nullptr) {
        return false;
    }
    const size_t len = strlen(s);
    size_t trimmed = len;
    while (trimmed > 0 && s[trimmed - 1] == ' ') {
        --trimmed;
    }
    return strncmp(s, expected, trimmed) == 0 && expected[trimmed] == '\0';
}
