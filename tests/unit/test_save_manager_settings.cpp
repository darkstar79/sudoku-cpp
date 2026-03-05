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

#include "../../src/core/save_manager.h"

#include <algorithm>
#include <chrono>
#include <filesystem>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
namespace fs = std::filesystem;

// RAII temporary directory
class TempDir {
public:
    TempDir()
        : path_(
              fs::temp_directory_path() /
              ("sudoku_settings_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()))) {
        fs::create_directories(path_);
    }
    ~TempDir() {
        if (fs::exists(path_)) {
            fs::remove_all(path_);
        }
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    [[nodiscard]] const fs::path& path() const {
        return path_;
    }

private:
    fs::path path_;
};

// Helper: minimal valid SavedGame (all 9x9 boards initialised)
static SavedGame makeValidGame() {
    SavedGame game;
    game.current_state.assign(9, std::vector<int>(9, 0));
    game.original_puzzle.assign(9, std::vector<int>(9, 0));
    game.notes.assign(9, std::vector<std::vector<int>>(9));
    game.hint_revealed_cells.assign(9, std::vector<bool>(9, false));
    game.difficulty = Difficulty::Easy;
    game.puzzle_seed = 1;
    game.display_name = "Test";
    return game;
}

TEST_CASE("SaveManager - Pre-existing save_id is reused", "[save_manager_settings]") {
    TempDir tmp;
    SaveManager mgr(tmp.path().string());

    SavedGame game = makeValidGame();
    game.save_id = "my-custom-id";

    SaveSettings settings;
    settings.compress = false;

    auto result = mgr.saveGame(game, settings);
    REQUIRE(result.has_value());
    // The returned save_id must match the one we set
    REQUIRE(*result == "my-custom-id");

    // And the file should exist at that id location
    REQUIRE(fs::exists(tmp.path() / "my-custom-id.yaml"));
}

TEST_CASE("SaveManager - is_complete=true is serialized correctly", "[save_manager_settings]") {
    TempDir tmp;
    SaveManager mgr(tmp.path().string());

    SavedGame game = makeValidGame();
    game.is_complete = true;

    SaveSettings settings;
    settings.compress = false;

    auto save_result = mgr.saveGame(game, settings);
    REQUIRE(save_result.has_value());

    auto load_result = mgr.loadGame(*save_result);
    REQUIRE(load_result.has_value());
    REQUIRE(load_result->is_complete == true);
}

TEST_CASE("SaveManager - Empty notes skips 'notes' key in YAML", "[save_manager_settings]") {
    TempDir tmp;
    SaveManager mgr(tmp.path().string());

    // All notes empty → has_notes remains false → no "notes" key serialized
    SavedGame game = makeValidGame();
    // notes already empty from makeValidGame()

    SaveSettings settings;
    settings.compress = false;

    auto save_result = mgr.saveGame(game, settings);
    REQUIRE(save_result.has_value());

    // Load succeeds — missing "notes" key is handled by deserializiation
    auto load_result = mgr.loadGame(*save_result);
    REQUIRE(load_result.has_value());
    // When no notes key was serialized, notes is left empty (not populated to 9×9)
    // Verify no note values exist by checking the flat structure
    bool any_note_found = false;
    for (const auto& row : load_result->notes) {
        for (const auto& cell : row) {
            if (!cell.empty()) {
                any_note_found = true;
            }
        }
    }
    REQUIRE_FALSE(any_note_found);
}

TEST_CASE("SaveManager - Non-empty notes: 'notes' key is serialized", "[save_manager_settings]") {
    TempDir tmp;
    SaveManager mgr(tmp.path().string());

    SavedGame game = makeValidGame();
    game.notes[0][0] = {1, 3, 5};

    SaveSettings settings;
    settings.compress = false;

    auto save_result = mgr.saveGame(game, settings);
    REQUIRE(save_result.has_value());

    auto load_result = mgr.loadGame(*save_result);
    REQUIRE(load_result.has_value());
    REQUIRE(load_result->notes[0][0] == std::vector<int>({1, 3, 5}));
}

TEST_CASE("SaveManager - include_history=true with non-empty move_history", "[save_manager_settings]") {
    TempDir tmp;
    SaveManager mgr(tmp.path().string());

    SavedGame game = makeValidGame();

    // Add a move to move_history
    Move move;
    move.position = Position{.row = 0, .col = 0};
    move.value = 5;
    move.move_type = MoveType::PlaceNumber;
    move.is_note = false;
    move.timestamp = std::chrono::steady_clock::now();
    game.move_history.push_back(move);
    game.current_move_index = 0;

    SaveSettings settings;
    settings.compress = false;
    settings.include_history = true;

    // This covers the true branch: settings.include_history && !move_history.empty()
    auto save_result = mgr.saveGame(game, settings);
    REQUIRE(save_result.has_value());
}

TEST_CASE("SaveManager - include_history=false with non-empty move_history", "[save_manager_settings]") {
    TempDir tmp;
    SaveManager mgr(tmp.path().string());

    SavedGame game = makeValidGame();

    Move move;
    move.position = Position{.row = 1, .col = 1};
    move.value = 3;
    move.move_type = MoveType::PlaceNumber;
    move.is_note = false;
    move.timestamp = std::chrono::steady_clock::now();
    game.move_history.push_back(move);

    SaveSettings settings;
    settings.compress = false;
    settings.include_history = false;

    // This covers the false branch: include_history is false → skip history block
    auto save_result = mgr.saveGame(game, settings);
    REQUIRE(save_result.has_value());
}

TEST_CASE("SaveManager - Invalid board dimensions return SerializationError", "[save_manager_settings]") {
    TempDir tmp;
    SaveManager mgr(tmp.path().string());

    SaveSettings settings;
    settings.compress = false;

    SECTION("current_state has wrong row count (8 rows)") {
        SavedGame game = makeValidGame();
        game.current_state.resize(8);  // Only 8 rows instead of 9

        auto result = mgr.saveGame(game, settings);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == SaveError::SerializationError);
    }

    SECTION("current_state row has wrong column count (8 cols)") {
        SavedGame game = makeValidGame();
        game.current_state[3].resize(8);  // Row 3 has only 8 cols

        auto result = mgr.saveGame(game, settings);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == SaveError::SerializationError);
    }

    SECTION("original_puzzle row has wrong column count") {
        SavedGame game = makeValidGame();
        game.original_puzzle[0].resize(8);  // Row 0 has only 8 cols

        auto result = mgr.saveGame(game, settings);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == SaveError::SerializationError);
    }
}
