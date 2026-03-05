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

#include "statistics_manager.h"

#include "core/constants.h"
#include "infrastructure/app_directory_manager.h"

#include <algorithm>
#include <fstream>
#include <string_view>
#include <tuple>
#include <utility>

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

namespace sudoku::core {

StatisticsManager::StatisticsManager(const std::string& stats_directory, std::shared_ptr<ITimeProvider> time_provider)
    : time_provider_(std::move(time_provider)), next_session_id_(1), stats_cache_valid_(false) {
    if (stats_directory.empty()) {
        // Use platform-appropriate default directory
        stats_directory_ =
            infrastructure::AppDirectoryManager::getDefaultDirectory(infrastructure::DirectoryType::Statistics);
    } else {
        stats_directory_ = stats_directory;
    }

    stats_file_ = stats_directory_ / "aggregate_stats.yaml";
    sessions_file_ = stats_directory_ / "game_sessions.yaml";

    auto result = ensureDirectoryExists();
    if (!result) {
        spdlog::warn("Failed to create stats directory: {}", stats_directory_.string());
    }

    // Load existing statistics
    auto load_result = loadStatistics();
    if (!load_result) {
        spdlog::warn("Failed to load existing statistics, starting fresh");
        // Initialize empty stats
        cached_stats_ = AggregateStats{};
        stats_cache_valid_ = true;
    }
}

StatisticsManager::~StatisticsManager() {
    // End any active sessions as incomplete
    for (auto& [session_id, stats] : active_sessions_) {
        stats.end_time = time_provider_->systemNow();
        stats.time_played = std::chrono::duration_cast<std::chrono::milliseconds>(stats.end_time - stats.start_time);
        stats.completed = false;

        std::ignore = saveGameSession(stats);
        updateAggregateStats(stats);
    }

    std::ignore = saveStatistics();
}

std::expected<uint64_t, StatisticsError> StatisticsManager::startGame(Difficulty difficulty, uint32_t puzzle_seed,
                                                                      int puzzle_rating) {
    if (!isValidDifficulty(difficulty)) {
        return std::unexpected(StatisticsError::InvalidDifficulty);
    }

    uint64_t session_id = generateSessionId();

    GameStats stats;
    stats.difficulty = difficulty;
    stats.puzzle_seed = puzzle_seed;
    stats.puzzle_rating = puzzle_rating;
    stats.start_time = time_provider_->systemNow();
    stats.completed = false;
    stats.moves_made = 0;
    stats.hints_used = 0;
    stats.mistakes = 0;

    active_sessions_[session_id] = stats;

    spdlog::debug("Started game session {} with difficulty {} (rating: {})", session_id, static_cast<int>(difficulty),
                  puzzle_rating);

    return session_id;
}

std::expected<void, StatisticsError> StatisticsManager::recordMove(uint64_t game_id, bool is_mistake) {
    if (!isValidGameSession(game_id)) {
        return std::unexpected(StatisticsError::GameNotStarted);
    }

    auto& stats = active_sessions_[game_id];
    stats.moves_made++;
    if (is_mistake) {
        stats.mistakes++;
    }

    spdlog::debug("Recorded move for session {}: moves={}, mistakes={}", game_id, stats.moves_made, stats.mistakes);

    return {};
}

std::expected<void, StatisticsError> StatisticsManager::recordHint(uint64_t game_id) {
    if (!isValidGameSession(game_id)) {
        return std::unexpected(StatisticsError::GameNotStarted);
    }

    auto& stats = active_sessions_[game_id];
    stats.hints_used++;

    spdlog::debug("Recorded hint for session {}: hints={}", game_id, stats.hints_used);

    return {};
}

std::expected<GameStats, StatisticsError> StatisticsManager::endGame(uint64_t game_id, bool completed) {
    if (!isValidGameSession(game_id)) {
        return std::unexpected(StatisticsError::GameNotStarted);
    }

    auto& stats = active_sessions_[game_id];
    stats.end_time = time_provider_->systemNow();
    stats.time_played = std::chrono::duration_cast<std::chrono::milliseconds>(stats.end_time - stats.start_time);
    stats.completed = completed;

    // Save the completed game session
    auto save_result = saveGameSession(stats);
    if (!save_result) {
        spdlog::warn("Failed to save game session {}", game_id);
    }

    // Update aggregate statistics
    updateAggregateStats(stats);

    // Save updated aggregate stats
    auto save_agg_result = saveStatistics();
    if (!save_agg_result) {
        spdlog::warn("Failed to save aggregate statistics");
    }

    GameStats result = stats;
    active_sessions_.erase(game_id);

    spdlog::info("Ended game session {}: completed={}, time={}ms, moves={}", game_id, completed,
                 result.time_played.count(), result.moves_made);

    return result;
}

std::expected<GameStats, StatisticsError> StatisticsManager::getGameStats(uint64_t game_id) const {
    auto it = active_sessions_.find(game_id);
    if (it == active_sessions_.end()) {
        return std::unexpected(StatisticsError::GameNotStarted);
    }

    return it->second;
}

std::expected<AggregateStats, StatisticsError> StatisticsManager::getAggregateStats() const {
    if (!stats_cache_valid_) {
        auto recalc_result = recalculateAggregateStats();
        if (!recalc_result) {
            return std::unexpected(recalc_result.error());
        }
    }

    return cached_stats_;
}

std::expected<AggregateStats, StatisticsError> StatisticsManager::getStatsForDifficulty(Difficulty difficulty) const {
    if (!isValidDifficulty(difficulty)) {
        return std::unexpected(StatisticsError::InvalidDifficulty);
    }

    auto all_stats_result = getAggregateStats();
    if (!all_stats_result) {
        return std::unexpected(all_stats_result.error());
    }

    const auto& all_stats = *all_stats_result;
    // Enum is 0-based (Easy=0, Medium=1, Hard=2, Expert=3), matches array indexing
    int diff_index = static_cast<int>(difficulty);

    AggregateStats difficulty_stats;
    difficulty_stats.games_played[diff_index] = all_stats.games_played[diff_index];
    difficulty_stats.games_completed[diff_index] = all_stats.games_completed[diff_index];
    difficulty_stats.best_times[diff_index] = all_stats.best_times[diff_index];
    difficulty_stats.average_times[diff_index] = all_stats.average_times[diff_index];

    // Calculate totals for this difficulty only
    difficulty_stats.total_games = all_stats.games_played[diff_index];
    difficulty_stats.total_completed = all_stats.games_completed[diff_index];

    return difficulty_stats;
}

std::expected<std::vector<GameStats>, StatisticsError> StatisticsManager::getRecentGames(int count) const {
    try {
        if (!std::filesystem::exists(sessions_file_)) {
            return std::vector<GameStats>{};  // Return empty if no sessions file
        }

        auto all_sessions_result = deserializeGameStatsFromYaml(sessions_file_);
        if (!all_sessions_result) {
            return std::unexpected(all_sessions_result.error());
        }

        auto sessions = *all_sessions_result;

        // Sort by start time (newest first)
        std::ranges::sort(sessions,
                          [](const GameStats& lhs, const GameStats& rhs) { return lhs.start_time > rhs.start_time; });

        // Return requested count
        if (count >= 0 && std::cmp_less(count, sessions.size())) {
            sessions.resize(count);
        }

        return sessions;

    } catch (const std::exception& e) {
        spdlog::error("Exception getting recent games: {}", e.what());
        return std::unexpected(StatisticsError::FileAccessError);
    }
}

std::array<std::chrono::milliseconds, 5> StatisticsManager::getBestTimes() const {
    auto stats_result = getAggregateStats();
    if (!stats_result) {
        return {};  // Return empty array on error
    }

    return stats_result->best_times;
}

std::array<double, 5> StatisticsManager::getCompletionRates() const {
    auto stats_result = getAggregateStats();
    if (!stats_result) {
        return {};  // Return empty array on error
    }

    const auto& stats = *stats_result;
    std::array<double, 5> rates{};

    for (size_t i = 0; i < DIFFICULTY_COUNT; ++i) {
        if (stats.games_played[i] > 0) {
            rates[i] = static_cast<double>(stats.games_completed[i]) / stats.games_played[i];
        } else {
            rates[i] = 0.0;
        }
    }

    return rates;
}

std::expected<void, StatisticsError> StatisticsManager::exportStats(const std::string& file_path) const {
    auto stats_result = getAggregateStats();
    if (!stats_result) {
        return std::unexpected(stats_result.error());
    }

    try {
        auto serialize_result = serializeStatsToYaml(*stats_result, file_path);
        if (!serialize_result) {
            return std::unexpected(serialize_result.error());
        }

        spdlog::info("Statistics exported to: {}", file_path);
        return {};

    } catch (const std::exception& e) {
        spdlog::error("Exception during stats export: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

std::expected<void, StatisticsError> StatisticsManager::exportAggregateStatsCsv(const std::string& file_path) const {
    auto stats_result = getAggregateStats();
    if (!stats_result) {
        return std::unexpected(stats_result.error());
    }

    const auto& stats = *stats_result;

    try {
        std::ofstream file(file_path);
        if (!file.is_open()) {
            spdlog::error("Failed to open CSV file for writing: {}", file_path);
            return std::unexpected(StatisticsError::FileAccessError);
        }

        // Write header row
        file << "Difficulty,Games_Played,Games_Completed,Completion_Rate,Best_Time_Seconds,Average_Time_Seconds,"
                "Average_Rating,Min_Rating,Max_Rating\n";

        // Difficulty names for CSV output
        constexpr std::array<std::string_view, DIFFICULTY_COUNT> csv_difficulty_names = {"Easy", "Medium", "Hard",
                                                                                         "Expert", "Master"};
        static_assert(csv_difficulty_names.size() == DIFFICULTY_COUNT,
                      "csv_difficulty_names size must match DIFFICULTY_COUNT");

        // Write per-difficulty rows
        for (size_t i = 0; i < DIFFICULTY_COUNT; ++i) {
            file << csv_difficulty_names[i] << ",";
            file << stats.games_played[i] << ",";
            file << stats.games_completed[i] << ",";

            // Calculate completion rate
            double completion_rate = 0.0;
            if (stats.games_played[i] > 0) {
                completion_rate = (stats.games_completed[i] / static_cast<double>(stats.games_played[i])) * 100.0;
            }
            file << completion_rate << ",";

            // Convert times from milliseconds to seconds
            double best_time_seconds = static_cast<double>(stats.best_times[i].count()) / 1000.0;
            double avg_time_seconds = static_cast<double>(stats.average_times[i].count()) / 1000.0;
            file << best_time_seconds << ",";
            file << avg_time_seconds << ",";

            // Rating statistics
            file << stats.average_ratings[i] << ",";
            file << (stats.min_ratings[i] == INT_MAX ? 0 : stats.min_ratings[i]) << ",";
            file << stats.max_ratings[i] << "\n";
        }

        // Write OVERALL row
        file << "OVERALL,";
        file << stats.total_games << ",";
        file << stats.total_completed << ",";
        double overall_completion_rate = 0.0;
        if (stats.total_games > 0) {
            overall_completion_rate = (stats.total_completed / static_cast<double>(stats.total_games)) * 100.0;
        }
        file << overall_completion_rate << ",";
        file << "N/A,N/A,N/A,N/A,N/A\n";  // N/A for time and rating columns

        // Write empty line separator
        file << "\n";

        // Write additional aggregate stats header
        file << "Total_Moves,Total_Hints,Total_Mistakes,Total_Time_Hours,Current_Streak,Best_Streak\n";

        // Calculate total time in hours
        double total_time_hours = static_cast<double>(stats.total_time_played.count()) / (1000.0 * 60.0 * 60.0);

        // Write aggregate stats row
        file << stats.total_moves << ",";
        file << stats.total_hints << ",";
        file << stats.total_mistakes << ",";
        file << total_time_hours << ",";
        file << stats.current_win_streak << ",";
        file << stats.best_win_streak << "\n";

        file.close();
        spdlog::info("Aggregate statistics exported to CSV: {}", file_path);
        return {};

    } catch (const std::exception& e) {
        spdlog::error("Exception during CSV export: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

std::expected<void, StatisticsError> StatisticsManager::exportGameSessionsCsv(const std::string& file_path) const {
    try {
        // Load all game sessions
        std::vector<GameStats> all_sessions;

        if (std::filesystem::exists(sessions_file_)) {
            auto sessions_result = deserializeGameStatsFromYaml(sessions_file_);
            if (!sessions_result) {
                return std::unexpected(sessions_result.error());
            }
            all_sessions = *sessions_result;
        }

        // Open CSV file for writing
        std::ofstream file(file_path);
        if (!file.is_open()) {
            spdlog::error("Failed to open CSV file for writing: {}", file_path);
            return std::unexpected(StatisticsError::FileAccessError);
        }

        // Write header row
        file << "Session_ID,Date,Time,Difficulty,Rating,Completed,Duration_Seconds,Moves,Hints,Mistakes,Puzzle_Seed\n";

        // Difficulty names for CSV output
        constexpr std::array<std::string_view, DIFFICULTY_COUNT> csv_difficulty_names = {"Easy", "Medium", "Hard",
                                                                                         "Expert", "Master"};
        static_assert(csv_difficulty_names.size() == DIFFICULTY_COUNT,
                      "csv_difficulty_names size must match DIFFICULTY_COUNT");

        // Write session data rows
        for (size_t i = 0; i < all_sessions.size(); ++i) {
            const auto& session = all_sessions[i];

            // Session ID (1-indexed)
            file << (i + 1) << ",";

            // Format timestamp as date and time
            auto time_t = std::chrono::system_clock::to_time_t(session.start_time);
            auto tm = *std::localtime(&time_t);

            // Date (YYYY-MM-DD)
            file << std::put_time(&tm, "%Y-%m-%d") << ",";

            // Time (HH:MM:SS)
            file << std::put_time(&tm, "%H:%M:%S") << ",";

            // Difficulty (enum is 0-based, matches array indexing)
            file << csv_difficulty_names[static_cast<size_t>(session.difficulty)] << ",";

            // Rating
            file << session.puzzle_rating << ",";

            // Completed status
            file << (session.completed ? "Yes" : "No") << ",";

            // Duration in seconds
            double duration_seconds = static_cast<double>(session.time_played.count()) / 1000.0;
            file << duration_seconds << ",";

            // Moves, hints, mistakes
            file << session.moves_made << ",";
            file << session.hints_used << ",";
            file << session.mistakes << ",";

            // Puzzle seed
            file << session.puzzle_seed << "\n";
        }

        file.close();
        spdlog::info("Game sessions exported to CSV: {} ({} sessions)", file_path, all_sessions.size());
        return {};

    } catch (const std::exception& e) {
        spdlog::error("Exception during CSV export: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

std::expected<void, StatisticsError> StatisticsManager::importStats(const std::string& file_path) {
    try {
        if (!std::filesystem::exists(file_path)) {
            return std::unexpected(StatisticsError::FileAccessError);
        }

        auto import_result = deserializeStatsFromYaml(file_path);
        if (!import_result) {
            return std::unexpected(import_result.error());
        }

        // Merge imported stats with current stats
        const auto& imported_stats = *import_result;

        for (int i = 0; i < 4; ++i) {
            cached_stats_.games_played[i] += imported_stats.games_played[i];
            cached_stats_.games_completed[i] += imported_stats.games_completed[i];

            // Update best times if imported time is better
            if (imported_stats.best_times[i] < cached_stats_.best_times[i]) {
                cached_stats_.best_times[i] = imported_stats.best_times[i];
            }

            // Recalculate average times (simplified approach)
            if (cached_stats_.games_completed[i] > 0) {
                auto total_time = cached_stats_.average_times[i] * cached_stats_.games_completed[i] +
                                  imported_stats.average_times[i] * imported_stats.games_completed[i];
                cached_stats_.average_times[i] = total_time / cached_stats_.games_completed[i];
            }
        }

        // Update totals
        cached_stats_.total_games += imported_stats.total_games;
        cached_stats_.total_completed += imported_stats.total_completed;
        cached_stats_.total_moves += imported_stats.total_moves;
        cached_stats_.total_hints += imported_stats.total_hints;
        cached_stats_.total_mistakes += imported_stats.total_mistakes;
        cached_stats_.total_time_played += imported_stats.total_time_played;

        // Update streaks (take the better values)
        cached_stats_.current_win_streak =
            std::max(cached_stats_.current_win_streak, imported_stats.current_win_streak);
        cached_stats_.best_win_streak = std::max(cached_stats_.best_win_streak, imported_stats.best_win_streak);

        stats_cache_valid_ = true;

        auto save_result = saveStatistics();
        if (!save_result) {
            spdlog::warn("Failed to save merged statistics");
        }

        spdlog::info("Statistics imported from: {}", file_path);
        return {};

    } catch (const std::exception& e) {
        spdlog::error("Exception during stats import: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

void StatisticsManager::resetAllStats() {
    // Clear active sessions
    active_sessions_.clear();

    // Reset aggregate stats
    cached_stats_ = AggregateStats{};
    stats_cache_valid_ = true;

    try {
        // Delete stats files
        if (std::filesystem::exists(stats_file_)) {
            std::filesystem::remove(stats_file_);
        }
        if (std::filesystem::exists(sessions_file_)) {
            std::filesystem::remove(sessions_file_);
        }

        spdlog::info("All statistics reset");

    } catch (const std::exception& e) {
        spdlog::error("Exception during stats reset: {}", e.what());
    }
}

// Private helper methods

uint64_t StatisticsManager::generateSessionId() {
    return next_session_id_++;
}

std::expected<void, StatisticsError> StatisticsManager::ensureDirectoryExists() const {
    try {
        if (!std::filesystem::exists(stats_directory_)) {
            std::filesystem::create_directories(stats_directory_);
        }
        return {};
    } catch (const std::exception& e) {
        spdlog::error("Failed to create directory {}: {}", stats_directory_.string(), e.what());
        return std::unexpected(StatisticsError::FileAccessError);
    }
}

std::expected<void, StatisticsError> StatisticsManager::loadStatistics() {
    try {
        if (std::filesystem::exists(stats_file_)) {
            auto stats_result = deserializeStatsFromYaml(stats_file_);
            if (stats_result) {
                cached_stats_ = *stats_result;
                stats_cache_valid_ = true;
                spdlog::debug("Loaded aggregate statistics from file");
            } else {
                spdlog::warn("Failed to load aggregate stats, will recalculate");
                auto recalc_result = recalculateAggregateStats();
                if (!recalc_result) {
                    return std::unexpected(recalc_result.error());
                }
            }
        } else {
            // No existing stats file, try to recalculate from sessions
            auto recalc_result = recalculateAggregateStats();
            if (!recalc_result) {
                // If recalculation fails, start with empty stats
                cached_stats_ = AggregateStats{};
                stats_cache_valid_ = true;
            }
        }

        return {};

    } catch (const std::exception& e) {
        spdlog::error("Exception loading statistics: {}", e.what());
        return std::unexpected(StatisticsError::FileAccessError);
    }
}

std::expected<void, StatisticsError> StatisticsManager::saveStatistics() const {
    try {
        if (!stats_cache_valid_) {
            return std::unexpected(StatisticsError::InvalidGameData);
        }

        auto serialize_result = serializeStatsToYaml(cached_stats_, stats_file_);
        if (!serialize_result) {
            return std::unexpected(serialize_result.error());
        }

        spdlog::debug("Saved aggregate statistics to file");
        return {};

    } catch (const std::exception& e) {
        spdlog::error("Exception saving statistics: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

std::expected<void, StatisticsError> StatisticsManager::saveGameSession(const GameStats& stats) const {
    try {
        auto serialize_result = serializeGameStatsToYaml(stats, sessions_file_, true);
        if (!serialize_result) {
            return std::unexpected(serialize_result.error());
        }

        spdlog::debug("Saved game session to file");
        return {};

    } catch (const std::exception& e) {
        spdlog::error("Exception saving game session: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

std::expected<void, StatisticsError>
StatisticsManager::serializeStatsToYaml(const AggregateStats& stats, const std::filesystem::path& file_path) const {
    try {
        YAML::Node root;

        root["version"] = "1.0";
        root["last_updated"] =
            std::chrono::duration_cast<std::chrono::seconds>(time_provider_->systemNow().time_since_epoch()).count();

        // Per-difficulty stats
        YAML::Node difficulties;
        constexpr std::array<const char*, DIFFICULTY_COUNT> difficulty_names = {"easy", "medium", "hard", "expert",
                                                                                "master"};
        static_assert(difficulty_names.size() == DIFFICULTY_COUNT, "difficulty_names size must match DIFFICULTY_COUNT");

        for (size_t i = 0; i < DIFFICULTY_COUNT; ++i) {
            YAML::Node diff_node;
            diff_node["games_played"] = stats.games_played[i];
            diff_node["games_completed"] = stats.games_completed[i];
            diff_node["best_time"] = stats.best_times[i].count();
            diff_node["average_time"] = stats.average_times[i].count();
            diff_node["average_rating"] = stats.average_ratings[i];
            diff_node["min_rating"] = stats.min_ratings[i];
            diff_node["max_rating"] = stats.max_ratings[i];
            difficulties[difficulty_names[i]] = diff_node;
        }
        root["difficulties"] = difficulties;

        // Overall stats
        YAML::Node overall;
        overall["total_games"] = stats.total_games;
        overall["total_completed"] = stats.total_completed;
        overall["total_moves"] = stats.total_moves;
        overall["total_hints"] = stats.total_hints;
        overall["total_mistakes"] = stats.total_mistakes;
        overall["total_time_played"] = stats.total_time_played.count();
        overall["current_win_streak"] = stats.current_win_streak;
        overall["best_win_streak"] = stats.best_win_streak;
        root["overall"] = overall;

        std::ofstream file(file_path);
        if (!file.is_open()) {
            return std::unexpected(StatisticsError::FileAccessError);
        }

        file << root;
        file.close();

        return {};

    } catch (const YAML::Exception& e) {
        spdlog::error("YAML stats serialization error: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    } catch (const std::exception& e) {
        spdlog::error("Stats serialization error: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

std::expected<AggregateStats, StatisticsError>
// NOLINTNEXTLINE(readability-function-cognitive-complexity) — parses all stats sections from YAML; each section is necessary; cannot split without adding indirection
StatisticsManager::deserializeStatsFromYaml(const std::filesystem::path& file_path) {
    try {
        YAML::Node root = YAML::LoadFile(file_path.string());

        AggregateStats stats;

        // Per-difficulty stats
        if (root["difficulties"]) {
            const auto& difficulties = root["difficulties"];
            constexpr std::array<const char*, DIFFICULTY_COUNT> difficulty_names = {"easy", "medium", "hard", "expert",
                                                                                    "master"};
            static_assert(difficulty_names.size() == DIFFICULTY_COUNT,
                          "difficulty_names size must match DIFFICULTY_COUNT");

            for (size_t i = 0; i < DIFFICULTY_COUNT; ++i) {
                if (difficulties[difficulty_names[i]]) {
                    const auto& diff_node = difficulties[difficulty_names[i]];

                    if (diff_node["games_played"]) {
                        stats.games_played[i] = diff_node["games_played"].as<int>();
                    }
                    if (diff_node["games_completed"]) {
                        stats.games_completed[i] = diff_node["games_completed"].as<int>();
                    }
                    if (diff_node["best_time"]) {
                        stats.best_times[i] = std::chrono::milliseconds(diff_node["best_time"].as<long long>());
                    }
                    if (diff_node["average_time"]) {
                        stats.average_times[i] = std::chrono::milliseconds(diff_node["average_time"].as<long long>());
                    }
                    if (diff_node["average_rating"]) {
                        stats.average_ratings[i] = diff_node["average_rating"].as<int>();
                    }
                    if (diff_node["min_rating"]) {
                        stats.min_ratings[i] = diff_node["min_rating"].as<int>();
                    }
                    if (diff_node["max_rating"]) {
                        stats.max_ratings[i] = diff_node["max_rating"].as<int>();
                    }
                }
            }
        }

        // Overall stats
        if (root["overall"]) {
            const auto& overall = root["overall"];

            if (overall["total_games"]) {
                stats.total_games = overall["total_games"].as<int>();
            }
            if (overall["total_completed"]) {
                stats.total_completed = overall["total_completed"].as<int>();
            }
            if (overall["total_moves"]) {
                stats.total_moves = overall["total_moves"].as<int>();
            }
            if (overall["total_hints"]) {
                stats.total_hints = overall["total_hints"].as<int>();
            }
            if (overall["total_mistakes"]) {
                stats.total_mistakes = overall["total_mistakes"].as<int>();
            }
            if (overall["total_time_played"]) {
                stats.total_time_played = std::chrono::milliseconds(overall["total_time_played"].as<long long>());
            }
            if (overall["current_win_streak"]) {
                stats.current_win_streak = overall["current_win_streak"].as<int>();
            }
            if (overall["best_win_streak"]) {
                stats.best_win_streak = overall["best_win_streak"].as<int>();
            }
        }

        return stats;

    } catch (const YAML::Exception& e) {
        spdlog::error("YAML stats deserialization error: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    } catch (const std::exception& e) {
        spdlog::error("Stats deserialization error: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

std::expected<void, StatisticsError> StatisticsManager::serializeGameStatsToYaml(const GameStats& stats,
                                                                                 const std::filesystem::path& file_path,
                                                                                 bool append) {
    try {
        YAML::Node sessions;

        // Load existing sessions if appending
        if (append && std::filesystem::exists(file_path)) {
            try {
                sessions = YAML::LoadFile(file_path.string());
            } catch (const YAML::Exception&) {
                // If loading fails, start with empty node
                sessions = YAML::Node(YAML::NodeType::Sequence);
            }
        } else {
            sessions = YAML::Node(YAML::NodeType::Sequence);
        }

        // Create new session entry
        YAML::Node session;
        session["difficulty"] = static_cast<int>(stats.difficulty);
        session["puzzle_rating"] = stats.puzzle_rating;
        session["start_time"] =
            std::chrono::duration_cast<std::chrono::seconds>(stats.start_time.time_since_epoch()).count();
        session["end_time"] =
            std::chrono::duration_cast<std::chrono::seconds>(stats.end_time.time_since_epoch()).count();
        session["time_played"] = stats.time_played.count();
        session["completed"] = stats.completed;
        session["moves_made"] = stats.moves_made;
        session["hints_used"] = stats.hints_used;
        session["mistakes"] = stats.mistakes;
        session["puzzle_seed"] = stats.puzzle_seed;

        sessions.push_back(session);

        std::ofstream file(file_path);
        if (!file.is_open()) {
            return std::unexpected(StatisticsError::FileAccessError);
        }

        file << sessions;
        file.close();

        return {};

    } catch (const YAML::Exception& e) {
        spdlog::error("YAML game stats serialization error: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    } catch (const std::exception& e) {
        spdlog::error("Game stats serialization error: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

std::expected<std::vector<GameStats>, StatisticsError>
StatisticsManager::deserializeGameStatsFromYaml(const std::filesystem::path& file_path) {
    try {
        YAML::Node sessions = YAML::LoadFile(file_path.string());
        std::vector<GameStats> result;

        for (const auto& session_node : sessions) {
            GameStats stats;

            if (session_node["difficulty"]) {
                stats.difficulty = static_cast<Difficulty>(session_node["difficulty"].as<int>());
            }
            if (session_node["puzzle_rating"]) {
                stats.puzzle_rating = session_node["puzzle_rating"].as<int>();
            }
            if (session_node["start_time"]) {
                auto seconds = session_node["start_time"].as<long long>();
                stats.start_time = std::chrono::system_clock::time_point(std::chrono::seconds(seconds));
            }
            if (session_node["end_time"]) {
                auto seconds = session_node["end_time"].as<long long>();
                stats.end_time = std::chrono::system_clock::time_point(std::chrono::seconds(seconds));
            }
            if (session_node["time_played"]) {
                stats.time_played = std::chrono::milliseconds(session_node["time_played"].as<long long>());
            }
            if (session_node["completed"]) {
                stats.completed = session_node["completed"].as<bool>();
            }
            if (session_node["moves_made"]) {
                stats.moves_made = session_node["moves_made"].as<int>();
            }
            if (session_node["hints_used"]) {
                stats.hints_used = session_node["hints_used"].as<int>();
            }
            if (session_node["mistakes"]) {
                stats.mistakes = session_node["mistakes"].as<int>();
            }
            if (session_node["puzzle_seed"]) {
                stats.puzzle_seed = session_node["puzzle_seed"].as<uint32_t>();
            }

            result.push_back(stats);
        }

        return result;

    } catch (const YAML::Exception& e) {
        spdlog::error("YAML game stats deserialization error: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    } catch (const std::exception& e) {
        spdlog::error("Game stats deserialization error: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

void StatisticsManager::updateAggregateStats(const GameStats& completed_game) {
    // Enum is 0-based (Easy=0, Medium=1, Hard=2, Expert=3), matches array indexing
    int diff_index = static_cast<int>(completed_game.difficulty);

    // Update rating statistics BEFORE incrementing games_played (track for all games, not just completed)
    if (completed_game.puzzle_rating > 0) {
        // Get count BEFORE incrementing
        auto games_count_before = cached_stats_.games_played[diff_index];

        // Update min rating
        cached_stats_.min_ratings[diff_index] =
            std::min(cached_stats_.min_ratings[diff_index], completed_game.puzzle_rating);

        // Update max rating
        cached_stats_.max_ratings[diff_index] =
            std::max(cached_stats_.max_ratings[diff_index], completed_game.puzzle_rating);

        // Update average rating
        auto current_rating_average = cached_stats_.average_ratings[diff_index];
        cached_stats_.average_ratings[diff_index] =
            (current_rating_average * games_count_before + completed_game.puzzle_rating) / (games_count_before + 1);
    }

    // Update per-difficulty stats
    cached_stats_.games_played[diff_index]++;
    if (completed_game.completed) {
        cached_stats_.games_completed[diff_index]++;

        // Update best time
        if (completed_game.time_played < cached_stats_.best_times[diff_index]) {
            cached_stats_.best_times[diff_index] = completed_game.time_played;
        }

        // Update average time
        auto current_average = cached_stats_.average_times[diff_index];
        auto completed_count = cached_stats_.games_completed[diff_index];
        cached_stats_.average_times[diff_index] =
            (current_average * (completed_count - 1) + completed_game.time_played) / completed_count;
    }

    // Update overall stats
    cached_stats_.total_games++;
    if (completed_game.completed) {
        cached_stats_.total_completed++;
        cached_stats_.current_win_streak++;
        cached_stats_.best_win_streak = std::max(cached_stats_.best_win_streak, cached_stats_.current_win_streak);
    } else {
        cached_stats_.current_win_streak = 0;
    }

    cached_stats_.total_moves += completed_game.moves_made;
    cached_stats_.total_hints += completed_game.hints_used;
    cached_stats_.total_mistakes += completed_game.mistakes;
    cached_stats_.total_time_played += completed_game.time_played;

    stats_cache_valid_ = true;
}

void StatisticsManager::invalidateStatsCache() {
    stats_cache_valid_ = false;
}

std::expected<void, StatisticsError> StatisticsManager::recalculateAggregateStats() const {
    try {
        if (!std::filesystem::exists(sessions_file_)) {
            // No sessions file, start with empty stats
            cached_stats_ = AggregateStats{};
            stats_cache_valid_ = true;
            return {};
        }

        auto sessions_result = deserializeGameStatsFromYaml(sessions_file_);
        if (!sessions_result) {
            return std::unexpected(sessions_result.error());
        }

        const auto& sessions = *sessions_result;

        // Reset stats
        cached_stats_ = AggregateStats{};

        // Recalculate from all sessions
        for (const auto& session : sessions) {
            const_cast<StatisticsManager*>(this)
                ->updateAggregateStats(  // NOLINT(cppcoreguidelines-pro-type-const-cast) — mutable cache update from const observer method
                    session);
        }

        stats_cache_valid_ = true;
        spdlog::debug("Recalculated aggregate statistics from {} sessions", sessions.size());

        return {};

    } catch (const std::exception& e) {
        spdlog::error("Exception during stats recalculation: {}", e.what());
        return std::unexpected(StatisticsError::SerializationError);
    }
}

bool StatisticsManager::isValidGameSession(uint64_t game_id) const {
    return active_sessions_.contains(game_id);
}

bool StatisticsManager::isValidDifficulty(Difficulty difficulty) {
    int diff_val = static_cast<int>(difficulty);
    // Difficulty enum is 0-based: Easy=0, Medium=1, Hard=2, Expert=3
    return diff_val >= 0 && diff_val <= 3;
}

}  // namespace sudoku::core