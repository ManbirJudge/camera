#include "utils.hpp"

std::string Utils::getHomeDir() {
    const char* home = std::getenv("HOME");

    if (home == nullptr) return "";
    return home;
}

bool Utils::makeDir(const std::string& path) {
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s", path.c_str());
    
    size_t len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;

    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
                if (errno != EEXIST) {
                    Log::e() << "Failed to create director \"" << tmp << "\" with error: " << errno;
                    return false;
                }
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
        if (errno != EEXIST) {
            Log::e() << "Failed to create director \"" << tmp << "\" with error: " << errno;
            return false;
        }
    }

    return true;
}

std::string Utils::getFormattedDt(const std::string& fmt) {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* now_local = std::localtime(&now_c);

    char buf[80];
    std::strftime(buf, sizeof(buf), fmt.c_str(), now_local);

    return std::string(buf);
}