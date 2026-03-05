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

#include "../../src/core/command_history.h"
#include "../../src/core/game_commands.h"
#include "../../src/core/game_validator.h"
#include "../../src/model/game_state.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace sudoku::model;

// Mock command for testing
class MockCommand : public ICommand {
public:
    MockCommand(const std::string& desc, bool valid_state = true) : description_(desc), valid_state_(valid_state) {
    }

    std::string getDescription() const override {
        return description_;
    }
    bool representsValidState() const override {
        return valid_state_;
    }

    bool isExecuted() const {
        return executed_;  // Access base class member
    }

protected:
    void executeImpl() override {
        // Execution logic (base class handles executed_ flag)
    }
    void undoImpl() override {
        // Undo logic (base class handles executed_ flag)
    }

private:
    std::string description_;
    bool valid_state_;
};

TEST_CASE("CommandHistory - Basic Operations", "[command_history]") {
    CommandHistory history;

    SECTION("Initial state") {
        REQUIRE_FALSE(history.canUndo());
        REQUIRE_FALSE(history.canRedo());
        REQUIRE(history.getUndoCount() == 0);
        REQUIRE(history.getRedoCount() == 0);
        REQUIRE(history.getHistorySize() == 0);
    }

    SECTION("Execute command") {
        auto cmd = std::make_unique<MockCommand>("Test Command");
        auto* cmd_ptr = cmd.get();

        history.executeCommand(std::move(cmd));

        REQUIRE(cmd_ptr->isExecuted());
        REQUIRE(history.canUndo());
        REQUIRE_FALSE(history.canRedo());
        REQUIRE(history.getUndoCount() == 1);
        REQUIRE(history.getRedoCount() == 0);
        REQUIRE(history.getHistorySize() == 1);
    }
}

TEST_CASE("CommandHistory - Undo/Redo", "[command_history]") {
    CommandHistory history;

    auto cmd1 = std::make_unique<MockCommand>("Command 1");
    auto cmd2 = std::make_unique<MockCommand>("Command 2");
    auto* cmd1_ptr = cmd1.get();
    auto* cmd2_ptr = cmd2.get();

    history.executeCommand(std::move(cmd1));
    history.executeCommand(std::move(cmd2));

    SECTION("Undo operations") {
        REQUIRE(history.undo());
        REQUIRE_FALSE(cmd2_ptr->isExecuted());
        REQUIRE(cmd1_ptr->isExecuted());

        REQUIRE(history.undo());
        REQUIRE_FALSE(cmd1_ptr->isExecuted());
        REQUIRE_FALSE(cmd2_ptr->isExecuted());

        REQUIRE_FALSE(history.undo());  // No more to undo
    }

    SECTION("Redo operations") {
        REQUIRE(history.undo());
        REQUIRE(history.undo());

        REQUIRE(history.redo());
        REQUIRE(cmd1_ptr->isExecuted());
        REQUIRE_FALSE(cmd2_ptr->isExecuted());

        REQUIRE(history.redo());
        REQUIRE(cmd1_ptr->isExecuted());
        REQUIRE(cmd2_ptr->isExecuted());

        REQUIRE_FALSE(history.redo());  // No more to redo
    }
}

TEST_CASE("CommandHistory - Undo to Valid State", "[command_history]") {
    CommandHistory history;

    auto valid_cmd = std::make_unique<MockCommand>("Valid Command", true);
    auto invalid_cmd1 = std::make_unique<MockCommand>("Invalid Command 1", false);
    auto invalid_cmd2 = std::make_unique<MockCommand>("Invalid Command 2", false);

    auto* valid_ptr = valid_cmd.get();
    auto* invalid1_ptr = invalid_cmd1.get();
    auto* invalid2_ptr = invalid_cmd2.get();

    history.executeCommand(std::move(valid_cmd));
    history.executeCommand(std::move(invalid_cmd1));
    history.executeCommand(std::move(invalid_cmd2));

    SECTION("Can undo to valid state") {
        REQUIRE(history.canUndoToValid());
        REQUIRE(history.undoToLastValid());

        // Should be back to state after valid command
        REQUIRE(valid_ptr->isExecuted());
        REQUIRE_FALSE(invalid1_ptr->isExecuted());
        REQUIRE_FALSE(invalid2_ptr->isExecuted());
    }

    SECTION("No valid state available") {
        auto all_invalid = std::make_unique<CommandHistory>();
        auto inv1 = std::make_unique<MockCommand>("Invalid 1", false);
        auto inv2 = std::make_unique<MockCommand>("Invalid 2", false);

        all_invalid->executeCommand(std::move(inv1));
        all_invalid->executeCommand(std::move(inv2));

        REQUIRE(all_invalid->canUndoToValid());  // Can undo to beginning
        REQUIRE(all_invalid->undoToLastValid());
        REQUIRE(all_invalid->getUndoCount() == 0);  // Should be at beginning
    }
}

TEST_CASE("CommandHistory - Real Game Commands", "[command_history]") {
    CommandHistory history;
    auto game_state = std::make_shared<GameState>();
    auto validator = std::make_shared<GameValidator>();

    SECTION("Place and undo number") {
        auto cmd = std::make_unique<PlaceNumberCommand>(game_state, validator, Position{.row = 0, .col = 0}, 5);

        history.executeCommand(std::move(cmd));

        REQUIRE(game_state->getCell(0, 0).value == 5);
        REQUIRE(history.canUndo());

        REQUIRE(history.undo());
        REQUIRE(game_state->getCell(0, 0).value == 0);
    }

    SECTION("Note operations") {
        auto add_note_cmd = std::make_unique<AddNoteCommand>(game_state, Position{.row = 0, .col = 0}, 3);
        auto remove_note_cmd = std::make_unique<RemoveNoteCommand>(game_state, Position{.row = 0, .col = 0}, 3);

        history.executeCommand(std::move(add_note_cmd));

        const auto& notes = game_state->getCell(0, 0).notes;
        REQUIRE(std::find(notes.begin(), notes.end(), 3) != notes.end());

        history.executeCommand(std::move(remove_note_cmd));
        REQUIRE(std::find(notes.begin(), notes.end(), 3) == notes.end());

        // Undo remove note
        REQUIRE(history.undo());
        REQUIRE(std::find(notes.begin(), notes.end(), 3) != notes.end());

        // Undo add note
        REQUIRE(history.undo());
        REQUIRE(std::find(notes.begin(), notes.end(), 3) == notes.end());
    }
}

TEST_CASE("CommandHistory - History Limits", "[command_history]") {
    CommandHistory history;
    history.setMaxHistorySize(3);

    // Add 5 commands
    for (int i = 0; i < 5; ++i) {
        auto cmd = std::make_unique<MockCommand>("Command " + std::to_string(i));
        history.executeCommand(std::move(cmd));
    }

    SECTION("History size limited") {
        REQUIRE(history.getHistorySize() == 3);
        REQUIRE(history.getUndoCount() == 3);
    }

    SECTION("Can still undo limited commands") {
        REQUIRE(history.undo());
        REQUIRE(history.undo());
        REQUIRE(history.undo());
        REQUIRE_FALSE(history.undo());  // No more available
    }
}

TEST_CASE("CommandHistory - Edge Cases", "[command_history]") {
    CommandHistory history;

    SECTION("Execute null command is ignored") {
        history.executeCommand(nullptr);
        REQUIRE(history.getHistorySize() == 0);
        REQUIRE_FALSE(history.canUndo());
    }

    SECTION("Clear resets history") {
        auto cmd1 = std::make_unique<MockCommand>("Command 1");
        auto cmd2 = std::make_unique<MockCommand>("Command 2");
        history.executeCommand(std::move(cmd1));
        history.executeCommand(std::move(cmd2));

        REQUIRE(history.getHistorySize() == 2);

        history.clear();

        REQUIRE(history.getHistorySize() == 0);
        REQUIRE(history.getUndoCount() == 0);
        REQUIRE(history.getRedoCount() == 0);
        REQUIRE_FALSE(history.canUndo());
        REQUIRE_FALSE(history.canRedo());
    }

    SECTION("Execute command in middle of history removes forward history") {
        auto cmd1 = std::make_unique<MockCommand>("Command 1");
        auto cmd2 = std::make_unique<MockCommand>("Command 2");
        auto cmd3 = std::make_unique<MockCommand>("Command 3");

        history.executeCommand(std::move(cmd1));
        history.executeCommand(std::move(cmd2));
        REQUIRE(history.undo());  // Go back one

        REQUIRE(history.canRedo());
        REQUIRE(history.getHistorySize() == 2);

        // Execute new command - should erase forward history
        history.executeCommand(std::move(cmd3));

        REQUIRE_FALSE(history.canRedo());
        REQUIRE(history.getHistorySize() == 2);
        REQUIRE(history.getUndoDescription() == "Command 3");
    }

    SECTION("Get descriptions when cannot undo/redo") {
        REQUIRE(history.getUndoDescription() == "");
        REQUIRE(history.getRedoDescription() == "");

        auto cmd = std::make_unique<MockCommand>("Test Command");
        history.executeCommand(std::move(cmd));

        REQUIRE(history.getUndoDescription() == "Test Command");
        REQUIRE(history.getRedoDescription() == "");

        REQUIRE(history.undo());

        REQUIRE(history.getUndoDescription() == "");
        REQUIRE(history.getRedoDescription() == "Test Command");
    }

    SECTION("Unlimited history size (max_size = 0)") {
        history.setMaxHistorySize(0);  // Unlimited

        for (int i = 0; i < 150; ++i) {
            auto cmd = std::make_unique<MockCommand>("Command " + std::to_string(i));
            history.executeCommand(std::move(cmd));
        }

        REQUIRE(history.getHistorySize() == 150);
    }

    SECTION("History trim adjusts current_index when at boundary") {
        history.setMaxHistorySize(10);

        // Add 15 commands
        for (int i = 0; i < 15; ++i) {
            auto cmd = std::make_unique<MockCommand>("Command " + std::to_string(i));
            history.executeCommand(std::move(cmd));
        }

        // Undo several commands to position current_index in middle
        REQUIRE(history.undo());
        REQUIRE(history.undo());
        REQUIRE(history.undo());

        REQUIRE(history.getHistorySize() == 10);

        // Add more commands to trigger trim with current_index adjustment
        for (int i = 0; i < 5; ++i) {
            auto cmd = std::make_unique<MockCommand>("New " + std::to_string(i));
            history.executeCommand(std::move(cmd));
        }

        REQUIRE(history.getHistorySize() == 10);
    }

    SECTION("undoToLastValid when already at valid state") {
        auto valid_cmd = std::make_unique<MockCommand>("Valid", true);
        history.executeCommand(std::move(valid_cmd));

        // Already at valid state
        REQUIRE_FALSE(history.canUndoToValid());
    }

    SECTION("undoToLastValid breaks on failed undo (edge case)") {
        // This is a theoretical edge case - in practice undo() always succeeds
        // when canUndo() is true, but testing the break condition
        auto valid_cmd = std::make_unique<MockCommand>("Valid", true);
        auto invalid_cmd = std::make_unique<MockCommand>("Invalid", false);

        history.executeCommand(std::move(valid_cmd));
        history.executeCommand(std::move(invalid_cmd));

        REQUIRE(history.undoToLastValid());
        REQUIRE(history.getUndoCount() == 1);
    }

    SECTION("undoToLastValid returns false when history is empty") {
        // canUndoToValid() returns false for empty history → early return false
        REQUIRE_FALSE(history.canUndoToValid());
        REQUIRE_FALSE(history.undoToLastValid());
    }

    SECTION("trimHistory sets current_index to 0 when it is less than excess") {
        // Set up: max=unlimited, add 4 commands, undo all the way back (current_index_=0)
        // Then reduce max to 3 → trimHistory fires with excess=1, current_index_(0) < excess(1)
        history.setMaxHistorySize(0);  // unlimited first

        for (int i = 0; i < 4; ++i) {
            auto cmd = std::make_unique<MockCommand>("Cmd " + std::to_string(i));
            history.executeCommand(std::move(cmd));
        }
        REQUIRE(history.getHistorySize() == 4);

        // Undo all 4 → current_index_ = 0
        REQUIRE(history.undo());
        REQUIRE(history.undo());
        REQUIRE(history.undo());
        REQUIRE(history.undo());
        REQUIRE(history.getUndoCount() == 0);

        // Reduce max to 3: excess=1, current_index_(0) < excess(1) → else branch: current_index_=0
        history.setMaxHistorySize(3);
        REQUIRE(history.getHistorySize() == 3);
        REQUIRE(history.getUndoCount() == 0);  // current_index_ stays 0
    }
}