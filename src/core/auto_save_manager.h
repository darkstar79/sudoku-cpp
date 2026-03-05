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

#include "../model/game_state.h"
#include "i_save_manager.h"
#include "i_time_provider.h"

#include <chrono>
#include <functional>
#include <memory>

namespace sudoku::core {

/// Manages automatic saving of game state at regular intervals
class AutoSaveManager {
public:
    using SaveCallback = std::function<void(bool success)>;

    AutoSaveManager(std::shared_ptr<sudoku::model::GameState> game_state, std::shared_ptr<ISaveManager> save_manager,
                    std::shared_ptr<ITimeProvider> time_provider = std::make_shared<SystemTimeProvider>());

    ~AutoSaveManager();

    // Delete copy operations (not copyable due to unique resource management)
    AutoSaveManager(const AutoSaveManager&) = delete;
    AutoSaveManager& operator=(const AutoSaveManager&) = delete;

    // Allow move operations
    AutoSaveManager(AutoSaveManager&&) noexcept = default;
    AutoSaveManager& operator=(AutoSaveManager&&) noexcept = default;

    /// Start auto-save with specified interval
    void start(std::chrono::milliseconds interval = std::chrono::minutes(2));

    /// Stop auto-save
    void stop();

    /// Check if auto-save is currently running
    [[nodiscard]] bool isRunning() const;

    /// Force an immediate auto-save
    void saveNow(const SaveCallback& callback = nullptr);

    /// Set the auto-save interval
    void setInterval(std::chrono::milliseconds interval);

    /// Get the current auto-save interval
    [[nodiscard]] std::chrono::milliseconds getInterval() const;

    /// Set callback for auto-save events
    void setAutoSaveCallback(const SaveCallback& callback);

    /// Get time since last auto-save
    [[nodiscard]] std::chrono::milliseconds getTimeSinceLastSave() const;

    /// Check if game state has changed since last save
    [[nodiscard]] bool hasUnsavedChanges() const;

    /// Mark current state as saved (called after manual save)
    void markAsSaved();

    /// Update auto-save state (call this regularly from main loop)
    void update();

private:
    // Dependencies
    std::shared_ptr<sudoku::model::GameState> game_state_;
    std::shared_ptr<ISaveManager> save_manager_;
    std::shared_ptr<ITimeProvider> time_provider_;

    bool is_running_{false};
    std::chrono::milliseconds interval_;
    std::chrono::steady_clock::time_point last_save_time_;
    std::chrono::steady_clock::time_point last_state_change_;

    SaveCallback auto_save_callback_;

    // Track game state for change detection
    int last_move_count_{0};
    std::chrono::milliseconds last_elapsed_time_;

    /// Check if the game state has changed
    [[nodiscard]] bool detectStateChange() const;

    /// Perform the actual auto-save operation
    void performAutoSave();
};

}  // namespace sudoku::core