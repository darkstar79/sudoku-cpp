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

#include "log_manager.h"
// #include "ConfigManager.h"  // Temporarily disabled
#include <chrono>
#include <iostream>

#include <fmt/chrono.h>
#include <spdlog/async.h>

namespace sudoku::infrastructure {

std::expected<void, LogError> LogManager::initialize(const LogConfig& config) {
    config_ = config;

    if (!config_.enable_logging) {
        return {};  // Logging disabled, nothing to do
    }

    // Create log directory
    auto dir_result = createLogDirectory();
    if (!dir_result) {
        return std::unexpected(dir_result.error());
    }

    // Setup logger
    auto logger_result = setupLogger();
    if (!logger_result) {
        return std::unexpected(logger_result.error());
    }

    // Set initial log level
    auto level_result = setLogLevel(config_.log_level);
    if (!level_result) {
        return std::unexpected(level_result.error());
    }

    session_start_ = std::chrono::system_clock::now();

    LOG_INFO("Logging system initialized successfully");
    LOG_INFO("Log directory: {}", config_.log_directory.string());
    LOG_INFO("Max files: {}, Max file size: {} bytes", config_.max_files, config_.max_file_size);

    return {};
}

std::expected<void, LogError> LogManager::initializeDefault() {
    LogConfig default_config;

    // Use simple default log directory
    default_config.log_directory = std::filesystem::current_path() / "logs";

    return initialize(default_config);
}

void LogManager::shutdown() {
    if (logger_) {
        LOG_SESSION_END();
        LOG_INFO("Logging system shutting down");
        logger_->flush();
        spdlog::shutdown();
        logger_.reset();
    }
}

std::expected<void, LogError> LogManager::setLogLevel(const std::string& level) {
    auto parsed_level = parseLogLevel(level);
    if (!parsed_level) {
        return std::unexpected(parsed_level.error());
    }

    if (logger_) {
        logger_->set_level(parsed_level.value());
        spdlog::set_level(parsed_level.value());
    }

    config_.log_level = level;
    return {};
}

void LogManager::flush() {
    if (logger_) {
        logger_->flush();
    }
}

int LogManager::cleanupOldLogs() const {
    if (!std::filesystem::exists(config_.log_directory)) {
        return 0;
    }

    std::vector<std::filesystem::directory_entry> log_files;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(config_.log_directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".log") {
                log_files.push_back(entry);
            }
        }
    } catch (...) {
        return 0;
    }

    // Sort by modification time (oldest first)
    std::ranges::sort(log_files, [](const auto& a, const auto& b) {
        try {
            return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
        } catch (...) {
            return false;
        }
    });

    int cleaned = 0;

    // Remove files if we exceed max_files
    while (log_files.size() > static_cast<size_t>(config_.max_files)) {
        try {
            std::filesystem::remove(log_files.front().path());
            log_files.erase(log_files.begin());
            ++cleaned;
        } catch (...) {
            break;
        }
    }

    return cleaned;
}

size_t LogManager::getLogDirectorySize() const {
    if (!std::filesystem::exists(config_.log_directory)) {
        return 0;
    }

    size_t total_size = 0;
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(config_.log_directory)) {
            if (entry.is_regular_file()) {
                total_size += entry.file_size();
            }
        }
    } catch (...) {
        return 0;
    }

    return total_size;
}

std::vector<std::filesystem::path> LogManager::getLogFiles() const {
    std::vector<std::filesystem::path> files;

    if (!std::filesystem::exists(config_.log_directory)) {
        return files;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(config_.log_directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".log") {
                files.push_back(entry.path());
            }
        }
    } catch (...) {  // NOLINT(bugprone-empty-catch)
        // Intentionally empty: return empty vector on filesystem error
    }

    return files;
}

void LogManager::logSessionStart() {
    if (!logger_) {
        return;
    }

    LOG_INFO("=====================================");
    LOG_INFO("SESSION START - Sudoku Application");
    LOG_INFO("Session ID: {}",
             std::chrono::duration_cast<std::chrono::milliseconds>(session_start_.time_since_epoch()).count());
    LOG_INFO("=====================================");
}

void LogManager::logSessionEnd() {
    if (!logger_) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - session_start_);

    LOG_INFO("=====================================");
    LOG_INFO("SESSION END - Duration: {} ms", duration.count());
    LOG_INFO("=====================================");
}

void LogManager::logGameStart(std::string_view difficulty) {
    LOG_INFO("Game started - Difficulty: {}", difficulty);
}

void LogManager::logGameEnd(bool completed, std::string_view time) {
    LOG_INFO("Game ended - Completed: {}, Time: {}", completed ? "Yes" : "No", time);
}

void LogManager::logError(std::string_view component, std::string_view error) {
    LOG_ERROR("[{}] Error: {}", component, error);
}

void LogManager::logPerformance(std::string_view operation, double duration_ms) {
    LOG_DEBUG("Performance - {}: {:.2f} ms", operation, duration_ms);
}

std::expected<spdlog::level::level_enum, LogError> LogManager::parseLogLevel(std::string_view level) {
    std::string lower_level(level);
    std::ranges::transform(lower_level, lower_level.begin(),
                           [](unsigned char chr) { return static_cast<char>(std::tolower(chr)); });

    if (lower_level == "trace") {
        return spdlog::level::trace;
    }
    if (lower_level == "debug") {
        return spdlog::level::debug;
    }
    if (lower_level == "info") {
        return spdlog::level::info;
    }
    if (lower_level == "warn" || lower_level == "warning") {
        return spdlog::level::warn;
    }
    if (lower_level == "error") {
        return spdlog::level::err;
    }
    if (lower_level == "critical") {
        return spdlog::level::critical;
    }
    if (lower_level == "off") {
        return spdlog::level::off;
    }

    return std::unexpected(LogError::InvalidLogLevel);
}

std::expected<void, LogError> LogManager::createLogDirectory() const {
    try {
        std::filesystem::create_directories(config_.log_directory);
        return {};
    } catch (...) {
        return std::unexpected(LogError::DirectoryCreationFailed);
    }
}

std::expected<void, LogError> LogManager::setupLogger() {
    try {
        std::vector<spdlog::sink_ptr> sinks;

        // Add file sink if enabled
        if (config_.log_to_file) {
            auto file_sink = createFileSink();
            if (file_sink) {
                sinks.push_back(file_sink);
            }
        }

        // Add console sink if enabled
        if (config_.log_to_console) {
            auto console_sink = createConsoleSink();
            if (console_sink) {
                sinks.push_back(console_sink);
            }
        }

        if (sinks.empty()) {
            return std::unexpected(LogError::InitializationFailed);
        }

        // Create logger with multiple sinks
        logger_ = std::make_shared<spdlog::logger>("sudoku", sinks.begin(), sinks.end());

        // Set pattern
        logger_->set_pattern(config_.pattern);

        // Set flush policy
        if (config_.flush_on_debug) {
            logger_->flush_on(spdlog::level::debug);
        }

        // Register as default logger
        spdlog::set_default_logger(logger_);

        return {};
    } catch (...) {
        return std::unexpected(LogError::InitializationFailed);
    }
}

std::string LogManager::generateLogFilename() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    std::tm tm_val{};
    if (localtime_r(&time_t_val, &tm_val) == nullptr) {
        return config_.log_filename_pattern;
    }
    auto timestamp = fmt::format("{:%Y%m%d_%H%M%S}", tm_val);

    // Replace {} in pattern with timestamp
    std::string filename = config_.log_filename_pattern;
    size_t pos = filename.find("{}");
    if (pos != std::string::npos) {
        filename.replace(pos, 2, timestamp);
    }

    return filename;
}

std::shared_ptr<spdlog::sinks::sink> LogManager::createFileSink() {
    try {
        std::string filename = generateLogFilename();
        std::filesystem::path full_path = config_.log_directory / filename;

        // Create rotating file sink
        auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(full_path.string(), config_.max_file_size,
                                                                           config_.max_files);

        return sink;
    } catch (...) {
        return nullptr;
    }
}

std::shared_ptr<spdlog::sinks::sink> LogManager::createConsoleSink() {
    try {
        auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        return sink;
    } catch (...) {
        return nullptr;
    }
}

LogManager& getLogManager() {
    static LogManager instance;
    return instance;
}

void initializeLogging(const LogConfig& config) {
    auto result = getLogManager().initialize(config);
    if (!result) {
        // Fallback to stderr if logging setup fails
        std::cerr << "Failed to initialize logging system\n";
    }
}

void shutdownLogging() {
    getLogManager().shutdown();
}

}  // namespace sudoku::infrastructure