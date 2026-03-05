// sudoku-cpp - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string_view>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace sudoku::infrastructure {

/// Log configuration settings
struct LogConfig {
    bool enable_logging{true};
    std::string log_level{"info"};  // trace, debug, info, warn, error, critical
    bool log_to_file{true};
    bool log_to_console{true};
    std::filesystem::path log_directory;
    std::string log_filename_pattern{"sudoku_{}.log"};
    size_t max_file_size{static_cast<size_t>(1024) * 1024 * 5};  // 5MB per file
    int max_files{20};                                           // 20 sessions as per requirement
    bool flush_on_debug{true};
    std::string pattern{"[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v"};
};

/// Error types for logging operations
enum class LogError : std::uint8_t {
    InitializationFailed,
    InvalidLogLevel,
    FileAccessError,
    DirectoryCreationFailed,
    InvalidConfiguration
};

/// Centralized logging manager using spdlog with rotating files
class LogManager {
public:
    LogManager() = default;
    ~LogManager() = default;
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;
    LogManager(LogManager&&) = delete;
    LogManager& operator=(LogManager&&) = delete;

    /// Initialize logging system with configuration
    [[nodiscard]] std::expected<void, LogError> initialize(const LogConfig& config);

    /// Initialize with default configuration
    [[nodiscard]] std::expected<void, LogError> initializeDefault();

    /// Shutdown logging system
    void shutdown();

    /// Update log level at runtime
    [[nodiscard]] std::expected<void, LogError> setLogLevel(const std::string& level);

    /// Get current logger instance
    [[nodiscard]] std::shared_ptr<spdlog::logger> getLogger() const {
        return logger_;
    }

    /// Check if logging is initialized
    [[nodiscard]] bool isInitialized() const {
        return logger_ != nullptr;
    }

    /// Force flush all log buffers
    void flush();

    /// Get current configuration
    [[nodiscard]] const LogConfig& getConfig() const {
        return config_;
    }

    /// Clean up old log files beyond retention policy
    [[nodiscard]] int cleanupOldLogs() const;

    /// Get total size of log directory
    [[nodiscard]] size_t getLogDirectorySize() const;

    /// Get list of current log files
    [[nodiscard]] std::vector<std::filesystem::path> getLogFiles() const;

    /// Create a session marker in the log
    void logSessionStart();
    void logSessionEnd();

    /// Log application events
    static void logGameStart(std::string_view difficulty);
    static void logGameEnd(bool completed, std::string_view time);
    static void logError(std::string_view component, std::string_view error);
    static void logPerformance(std::string_view operation, double duration_ms);

private:
    LogConfig config_;
    std::shared_ptr<spdlog::logger> logger_;
    std::chrono::system_clock::time_point session_start_;

    // Helper methods
    [[nodiscard]] static std::expected<spdlog::level::level_enum, LogError> parseLogLevel(std::string_view level);
    [[nodiscard]] std::expected<void, LogError> createLogDirectory() const;
    [[nodiscard]] std::expected<void, LogError> setupLogger();
    [[nodiscard]] std::string generateLogFilename() const;

    // Sink creation helpers
    [[nodiscard]] std::shared_ptr<spdlog::sinks::sink> createFileSink();
    [[nodiscard]] static std::shared_ptr<spdlog::sinks::sink> createConsoleSink();
};

/// Global log manager instance
LogManager& getLogManager();

/// Convenience functions for common logging operations
void initializeLogging(const LogConfig& config = {});
void shutdownLogging();

/// Convenience macros for logging
#define LOG_TRACE(...)                                                                                                 \
    if (auto logger = sudoku::infrastructure::getLogManager().getLogger())                                             \
    logger->trace(__VA_ARGS__)
#define LOG_DEBUG(...)                                                                                                 \
    if (auto logger = sudoku::infrastructure::getLogManager().getLogger())                                             \
    logger->debug(__VA_ARGS__)
#define LOG_INFO(...)                                                                                                  \
    if (auto logger = sudoku::infrastructure::getLogManager().getLogger())                                             \
    logger->info(__VA_ARGS__)
#define LOG_WARN(...)                                                                                                  \
    if (auto logger = sudoku::infrastructure::getLogManager().getLogger())                                             \
    logger->warn(__VA_ARGS__)
#define LOG_ERROR(...)                                                                                                 \
    if (auto logger = sudoku::infrastructure::getLogManager().getLogger())                                             \
    logger->error(__VA_ARGS__)
#define LOG_CRITICAL(...)                                                                                              \
    if (auto logger = sudoku::infrastructure::getLogManager().getLogger())                                             \
    logger->critical(__VA_ARGS__)

/// Session logging macros
#define LOG_SESSION_START() sudoku::infrastructure::getLogManager().logSessionStart()
#define LOG_SESSION_END() sudoku::infrastructure::getLogManager().logSessionEnd()
#define LOG_GAME_START(difficulty) sudoku::infrastructure::getLogManager().logGameStart(difficulty)
#define LOG_GAME_END(completed, time) sudoku::infrastructure::getLogManager().logGameEnd(completed, time)
#define LOG_COMPONENT_ERROR(component, error) sudoku::infrastructure::getLogManager().logError(component, error)
#define LOG_PERFORMANCE(operation, duration) sudoku::infrastructure::getLogManager().logPerformance(operation, duration)

}  // namespace sudoku::infrastructure