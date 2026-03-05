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

#include "auto_save_manager.h"

namespace sudoku::core {

AutoSaveManager::AutoSaveManager(std::shared_ptr<sudoku::model::GameState> game_state,
                                 std::shared_ptr<ISaveManager> save_manager,
                                 std::shared_ptr<ITimeProvider> time_provider)
    : game_state_(std::move(game_state)), save_manager_(std::move(save_manager)),
      time_provider_(std::move(time_provider)), interval_(std::chrono::minutes(2)),
      last_save_time_(time_provider_->steadyNow()), last_state_change_(time_provider_->steadyNow()),
      last_elapsed_time_(std::chrono::milliseconds(0)) {
    if (game_state_) {
        last_move_count_ = game_state_->getMoveCount();
        last_elapsed_time_ = game_state_->getElapsedTime();
    }
}

AutoSaveManager::~AutoSaveManager() {
    stop();
}

void AutoSaveManager::start(std::chrono::milliseconds interval) {
    interval_ = interval;
    is_running_ = true;
    last_save_time_ = time_provider_->steadyNow();
}

void AutoSaveManager::stop() {
    is_running_ = false;
}

bool AutoSaveManager::isRunning() const {
    return is_running_;
}

void AutoSaveManager::saveNow(const SaveCallback& callback) {
    if (!game_state_ || !save_manager_) {
        if (callback) {
            callback(false);
        }
        return;
    }

    // Create auto-save file name with timestamp
    auto now = time_provider_->systemNow();

    // Create save data
    SavedGame save_data;
    save_data.display_name = "Auto Save";
    save_data.created_time = now;
    save_data.last_modified = now;
    save_data.difficulty = game_state_->getDifficulty();
    save_data.moves_made = game_state_->getMoveCount();
    save_data.elapsed_time = game_state_->getElapsedTime();
    save_data.is_complete = game_state_->isComplete();
    save_data.current_state = game_state_->extractNumbers();
    save_data.is_auto_save = true;
    save_data.last_auto_save = now;

    // Perform the save using autoSave method for auto-saves
    auto result = save_manager_->autoSave(save_data);
    bool success = result.has_value();

    if (success) {
        last_save_time_ = time_provider_->steadyNow();
        markAsSaved();
    }

    if (callback) {
        callback(success);
    }
}

void AutoSaveManager::setInterval(std::chrono::milliseconds interval) {
    interval_ = interval;
}

std::chrono::milliseconds AutoSaveManager::getInterval() const {
    return interval_;
}

void AutoSaveManager::setAutoSaveCallback(const SaveCallback& callback) {
    auto_save_callback_ = callback;
}

std::chrono::milliseconds AutoSaveManager::getTimeSinceLastSave() const {
    auto now = time_provider_->steadyNow();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - last_save_time_);
}

bool AutoSaveManager::hasUnsavedChanges() const {
    return detectStateChange();
}

void AutoSaveManager::markAsSaved() {
    if (game_state_) {
        last_move_count_ = game_state_->getMoveCount();
        last_elapsed_time_ = game_state_->getElapsedTime();
        last_state_change_ = time_provider_->steadyNow();
    }
}

void AutoSaveManager::update() {
    if (!is_running_ || !game_state_ || !save_manager_) {
        return;
    }

    // Check if state has changed
    if (detectStateChange()) {
        last_state_change_ = time_provider_->steadyNow();
    }

    // Check if it's time to auto-save
    auto time_since_save = getTimeSinceLastSave();
    bool should_save = time_since_save >= interval_ && hasUnsavedChanges();

    if (should_save) {
        performAutoSave();
    }
}

bool AutoSaveManager::detectStateChange() const {
    if (!game_state_) {
        return false;
    }

    // Check for move count changes
    if (game_state_->getMoveCount() != last_move_count_) {
        return true;
    }

    // Check for elapsed time changes (indicates timer activity)
    if (game_state_->getElapsedTime() != last_elapsed_time_) {
        return true;
    }

    return false;
}

void AutoSaveManager::performAutoSave() {
    saveNow(auto_save_callback_);
}

}  // namespace sudoku::core