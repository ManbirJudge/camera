#ifndef LOG_H
#define LOG_H

#include <string>
#include <sstream>
#include <iostream>

class Log {
public:
    enum Level { DEBUG, INFO, WARN, ERROR };

    class LogStream {
    private:
        std::ostringstream stream;

    public:
        LogStream(Level lvl) {
            switch (lvl)
            {
                case DEBUG: std::cout << "[DEBUG] "; break;
                case INFO : std::cout << "[ INFO] "; break;
                case WARN : std::cout << "[ WARN] "; break;
                case ERROR: std::cout << "[ERROR] "; break;
            }
        }

        LogStream(LogStream&& other) noexcept
            : stream(std::move(other.stream)) {}

        LogStream(const LogStream&) = delete;

        ~LogStream() {
            std::cout << std::endl;
        }

        template <typename T>
        LogStream& operator<<(const T& msg) {
            std::cout << msg;
            return *this;
        }
    };

    static LogStream d() { return LogStream(DEBUG); };
    static LogStream i() { return LogStream(INFO ); };
    static LogStream w() { return LogStream(WARN ); };
    static LogStream e() { return LogStream(ERROR); };
};

#endif