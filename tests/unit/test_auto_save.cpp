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

#include "../../src/core/auto_save_manager.h"
#include "../../src/core/i_time_provider.h"
#include "../../src/model/game_state.h"

#include <chrono>
#include <memory>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace sudoku::model;

// Mock save manager for testing
class MockSaveManager : public ISaveManager {
public:
    MockSaveManager() : save_called_(false), save_success_(true) {
    }

    std::expected<std::string, SaveError> saveGame([[maybe_unused]] const SavedGame& game,
                                                   [[maybe_unused]] const SaveSettings& settings = {}) override {
        // Not used in auto-save tests
        return "test_save_id";
    }

    std::expected<SavedGame, SaveError> loadGame([[maybe_unused]] const std::string& save_id) const override {
        // Not needed for auto-save tests
        return std::unexpected(SaveError::FileNotFound);
    }

    std::expected<void, SaveError> autoSave(const SavedGame& game) override {
        save_called_ = true;
        last_save_data_ = game;

        if (save_success_) {
            return {};
        } else {
            return std::unexpected(SaveError::FileAccessError);
        }
    }

    std::expected<SavedGame, SaveError> loadAutoSave() override {
        return std::unexpected(SaveError::FileNotFound);
    }

    bool hasAutoSave() const override {
        return false;
    }

    std::expected<void, SaveError> deleteSave([[maybe_unused]] const std::string& save_id) override {
        return {};
    }

    std::expected<std::vector<SavedGame>, SaveError> listSaves() const override {
        return std::vector<SavedGame>{};
    }

    std::expected<SavedGame, SaveError> getSaveInfo([[maybe_unused]] const std::string& save_id) const override {
        return std::unexpected(SaveError::FileNotFound);
    }

    std::expected<void, SaveError> renameSave([[maybe_unused]] const std::string& save_id,
                                              [[maybe_unused]] const std::string& new_name) override {
        return {};
    }

    std::expected<void, SaveError> exportSave([[maybe_unused]] const std::string& save_id,
                                              [[maybe_unused]] const std::string& export_path) const override {
        return {};
    }

    std::expected<std::string, SaveError>
    importSave([[maybe_unused]] const std::string& import_path,
               [[maybe_unused]] const std::optional<std::string>& custom_name) override {
        return "imported_save_id";
    }

    std::string getSaveDirectory() const override {
        return "/tmp/saves";
    }

    bool validateSave([[maybe_unused]] const std::string& save_id) const override {
        return true;
    }

    int cleanupOldSaves([[maybe_unused]] int days_old = 30) override {
        return 0;
    }

    uint64_t getSaveDirectorySize() const override {
        return 1024;
    }

    bool wasSaveCalled() const {
        return save_called_;
    }
    const SavedGame& getLastSaveData() const {
        return last_save_data_;
    }
    void setSaveSuccess(bool success) {
        save_success_ = success;
    }
    void reset() {
        save_called_ = false;
    }

private:
    bool save_called_;
    bool save_success_;
    SavedGame last_save_data_;
};

TEST_CASE("AutoSaveManager - Basic Operations", "[auto_save]") {
    auto game_state = std::make_shared<GameState>();
    auto mock_save_manager = std::make_shared<MockSaveManager>();
    AutoSaveManager auto_save(game_state, mock_save_manager);

    SECTION("Initial state") {
        REQUIRE_FALSE(auto_save.isRunning());
        REQUIRE(auto_save.getInterval() == std::chrono::minutes(2));
    }

    SECTION("Start and stop") {
        auto_save.start(std::chrono::milliseconds(100));
        REQUIRE(auto_save.isRunning());
        REQUIRE(auto_save.getInterval() == std::chrono::milliseconds(100));

        auto_save.stop();
        REQUIRE_FALSE(auto_save.isRunning());
    }

    SECTION("setInterval changes the interval") {
        REQUIRE(auto_save.getInterval() == std::chrono::minutes(2));
        auto_save.setInterval(std::chrono::milliseconds(500));
        REQUIRE(auto_save.getInterval() == std::chrono::milliseconds(500));
    }
}

TEST_CASE("AutoSaveManager - Manual Save", "[auto_save]") {
    auto game_state = std::make_shared<GameState>();
    auto mock_save_manager = std::make_shared<MockSaveManager>();
    AutoSaveManager auto_save(game_state, mock_save_manager);

    SECTION("Save now") {
        bool callback_called = false;
        bool save_result = false;

        auto_save.saveNow([&](bool success) {
            callback_called = true;
            save_result = success;
        });

        REQUIRE(mock_save_manager->wasSaveCalled());
        REQUIRE(callback_called);
        REQUIRE(save_result);
        REQUIRE_FALSE(auto_save.hasUnsavedChanges());
    }

    SECTION("Save failure") {
        mock_save_manager->setSaveSuccess(false);

        bool callback_called = false;
        bool save_result = true;  // Should become false

        auto_save.saveNow([&](bool success) {
            callback_called = true;
            save_result = success;
        });

        REQUIRE(mock_save_manager->wasSaveCalled());
        REQUIRE(callback_called);
        REQUIRE_FALSE(save_result);
    }
}

TEST_CASE("AutoSaveManager - Change Detection", "[auto_save]") {
    // Use MockTimeProvider for timer tests
    auto mock_time = std::make_shared<MockTimeProvider>();
    auto game_state = std::make_shared<GameState>(mock_time);
    auto mock_save_manager = std::make_shared<MockSaveManager>();
    AutoSaveManager auto_save(game_state, mock_save_manager, mock_time);

    SECTION("Detect move count changes") {
        REQUIRE_FALSE(auto_save.hasUnsavedChanges());

        game_state->incrementMoves();
        REQUIRE(auto_save.hasUnsavedChanges());

        auto_save.markAsSaved();
        REQUIRE_FALSE(auto_save.hasUnsavedChanges());
    }

    SECTION("Detect timer changes") {
        REQUIRE_FALSE(auto_save.hasUnsavedChanges());

        game_state->startTimer();
        // Advance time so elapsed time changes
        mock_time->advanceSystemTime(std::chrono::milliseconds(10));

        REQUIRE(auto_save.hasUnsavedChanges());
    }
}

TEST_CASE("AutoSaveManager - Auto Save Timing", "[auto_save]") {
    // Use MockTimeProvider for deterministic timing
    auto mock_time = std::make_shared<MockTimeProvider>();
    auto game_state = std::make_shared<GameState>(mock_time);
    auto mock_save_manager = std::make_shared<MockSaveManager>();
    AutoSaveManager auto_save(game_state, mock_save_manager, mock_time);

    SECTION("Auto save after interval") {
        // Set interval for testing
        auto_save.start(std::chrono::milliseconds(100));

        // Make a change
        game_state->incrementMoves();

        // Update should not trigger save immediately (interval hasn't passed)
        auto_save.update();
        REQUIRE_FALSE(mock_save_manager->wasSaveCalled());

        // Advance time past interval
        mock_time->advanceSteadyTime(std::chrono::milliseconds(150));

        // Now update should trigger save
        auto_save.update();
        REQUIRE(mock_save_manager->wasSaveCalled());
    }

    SECTION("No auto save without changes") {
        auto_save.start(std::chrono::milliseconds(100));

        // Advance time past interval but don't make changes
        mock_time->advanceSteadyTime(std::chrono::milliseconds(150));
        auto_save.update();

        REQUIRE_FALSE(mock_save_manager->wasSaveCalled());
    }
}

TEST_CASE("AutoSaveManager - Null Dependencies", "[auto_save]") {
    auto mock_time = std::make_shared<MockTimeProvider>();
    auto mock_save_manager = std::make_shared<MockSaveManager>();

    SECTION("markAsSaved() with null game_state is a no-op") {
        AutoSaveManager auto_save(nullptr, mock_save_manager, mock_time);
        // Should not crash — the if (game_state_) false branch
        REQUIRE_NOTHROW(auto_save.markAsSaved());
    }

    SECTION("update() when not started is a no-op") {
        auto game_state = std::make_shared<GameState>(mock_time);
        AutoSaveManager auto_save(game_state, mock_save_manager, mock_time);
        // Never call start() — is_running_ stays false
        game_state->incrementMoves();
        mock_time->advanceSteadyTime(std::chrono::milliseconds(500));
        auto_save.update();
        REQUIRE_FALSE(mock_save_manager->wasSaveCalled());
    }

    SECTION("hasUnsavedChanges() with null game_state returns false") {
        AutoSaveManager auto_save(nullptr, mock_save_manager, mock_time);
        // detectStateChange() returns false when game_state_ is null
        REQUIRE_FALSE(auto_save.hasUnsavedChanges());
    }

    SECTION("saveNow() with null game_state and no callback does not crash") {
        AutoSaveManager auto_save(nullptr, mock_save_manager, mock_time);
        // Hits if (!game_state_) early return AND if (callback) false branch
        REQUIRE_NOTHROW(auto_save.saveNow());
        REQUIRE_FALSE(mock_save_manager->wasSaveCalled());
    }

    SECTION("saveNow() with null game_state invokes callback with false") {
        AutoSaveManager auto_save(nullptr, mock_save_manager, mock_time);
        bool callback_called = false;
        bool callback_result = true;
        auto_save.saveNow([&](bool success) {
            callback_called = true;
            callback_result = success;
        });
        REQUIRE(callback_called);
        REQUIRE_FALSE(callback_result);
        REQUIRE_FALSE(mock_save_manager->wasSaveCalled());
    }
}

TEST_CASE("AutoSaveManager - Callback Integration", "[auto_save]") {
    // Use MockTimeProvider for deterministic timing
    auto mock_time = std::make_shared<MockTimeProvider>();
    auto game_state = std::make_shared<GameState>(mock_time);
    auto mock_save_manager = std::make_shared<MockSaveManager>();
    AutoSaveManager auto_save(game_state, mock_save_manager, mock_time);

    SECTION("Auto save callback") {
        bool callback_executed = false;
        bool callback_result = false;

        auto_save.setAutoSaveCallback([&](bool success) {
            callback_executed = true;
            callback_result = success;
        });

        auto_save.start(std::chrono::milliseconds(50));
        game_state->incrementMoves();

        // Advance time past interval
        mock_time->advanceSteadyTime(std::chrono::milliseconds(60));
        auto_save.update();

        REQUIRE(callback_executed);
        REQUIRE(callback_result);
    }
}