#pragma once

#include <cstdint>
#include <cstdio>
#include <string_view>

namespace cppfig {

/// @brief Log levels for configuration messages.
enum class LogLevel : std::uint8_t { Info,
                                     Warning,
                                     Error };

/// @brief Simple logger that writes to stdout/stderr.
class Logger {
public:
    /// @brief Logs an info message to stdout.
    static void Info(std::string_view message) { std::fprintf(stdout, "[cppfig] INFO: %.*s\n", static_cast<int>(message.size()), message.data()); }

    /// @brief Logs a warning message to stderr.
    static void Warn(std::string_view message)
    {
        std::fprintf(stderr, "[cppfig] WARN: %.*s\n", static_cast<int>(message.size()), message.data());
    }

    /// @brief Logs an error message to stderr.
    static void Error(std::string_view message)
    {
        std::fprintf(stderr, "[cppfig] ERROR: %.*s\n", static_cast<int>(message.size()), message.data());
    }

    /// @brief Logs a message at the specified level.
    static void Log(LogLevel level, std::string_view message)
    {
        switch (level) {
        case LogLevel::Info:
            Info(message);
            break;
        case LogLevel::Warning:
            Warn(message);
            break;
        case LogLevel::Error:
            Error(message);
            break;
        }
    }

    /// @brief Logs a formatted info message to stdout.
    template <typename... Args>
    static void InfoF(const char* format, Args... args)
    {
        std::fprintf(stdout, "[cppfig] INFO: ");
        std::fprintf(stdout, format, args...);
        std::fprintf(stdout, "\n");
    }

    /// @brief Logs a formatted warning message to stderr.
    template <typename... Args>
    static void WarnF(const char* format, Args... args)
    {
        std::fprintf(stderr, "[cppfig] WARN: ");
        std::fprintf(stderr, format, args...);
        std::fprintf(stderr, "\n");
    }

    /// @brief Logs a formatted error message to stderr.
    template <typename... Args>
    static void ErrorF(const char* format, Args... args)
    {
        std::fprintf(stderr, "[cppfig] ERROR: ");
        std::fprintf(stderr, format, args...);
        std::fprintf(stderr, "\n");
    }
};

}  // namespace cppfig
