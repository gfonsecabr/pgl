#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <string_view>

namespace pgl_benchmark {

inline std::string canonicalNumber(std::string_view value) {
    std::string canonical;
    canonical.reserve(value.size());

    for (const unsigned char character : value) {
        if (std::isalnum(character)) {
            canonical.push_back(static_cast<char>(std::tolower(character)));
        }
    }

    if (canonical == "rationali64" || canonical == "rational64") {
        return "rational";
    }
    if (canonical == "rational60") {
        return "rational60";
    }
    return canonical;
}

inline bool numberEnabled(std::string_view number) {
    const char* raw_filter = std::getenv("PGL_BENCHMARK_NUMBERS");
    if (raw_filter == nullptr || *raw_filter == '\0') {
        return true;
    }

    const std::string filter(raw_filter);
    const std::string expected = canonicalNumber(number);
    std::string token;

    for (const char character : filter) {
        if (character == ',' || character == ';' || std::isspace(static_cast<unsigned char>(character))) {
            if (!token.empty() && canonicalNumber(token) == expected) {
                return true;
            }
            token.clear();
        } else {
            token.push_back(character);
        }
    }

    return !token.empty() && canonicalNumber(token) == expected;
}

}  // namespace pgl_benchmark
