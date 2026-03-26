#ifndef UTILS_H
#define UTILS_H

#include <chrono>
#include <cstring>
#include <ctime>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include <sys/stat.h>

#include "log.hpp"

namespace Utils {
    template <typename T>
    std::string join(
        const std::vector<T>& items,
        const std::string& sep,
        std::function<std::string(const T&)> get
    ) {
        std::ostringstream oss;
        for (size_t i = 0, n = items.size(); i < n; i++) {
            oss << get(items[i]);
            if (i != n - 1) oss << sep;
        }
        return oss.str();
    }
    
    std::string getHomeDir();
    bool makeDir(const std::string& path);

    std::string getFormattedDt(const std::string& fmt);
}

#endif