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

#include "../../src/core/game_commands.h"
#include "../../src/core/game_validator.h"
#include "../../src/model/game_state.h"

#include <memory>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::core;
using namespace sudoku::model;

TEST_CASE("PlaceNumberCommand - Edge Cases", "[game_commands][place_number]") {
    auto game_state = std::make_shared<GameState>();
    auto validator = std::make_shared<GameValidator>();

    SECTION("Execute on empty cell") {
        Position pos{.row = 0, .col = 0};
        PlaceNumberCommand cmd(game_state, validator, pos, 5);

        REQUIRE(cmd.canUndo() == false);

        cmd.execute();

        REQUIRE(game_state->getCell(pos).value == 5);
        REQUIRE(cmd.canUndo() == true);
        REQUIRE(cmd.getDescription() == "Place 5 at (0,0)");
    }

    SECTION("Double execute does nothing") {
        Position pos{.row = 1, .col = 1};
        PlaceNumberCommand cmd(game_state, validator, pos, 7);

        cmd.execute();
        REQUIRE(game_state->getCell(pos).value == 7);

        // Second execute should not change anything
        cmd.execute();
        REQUIRE(game_state->getCell(pos).value == 7);
    }

    SECTION("Undo without execute does nothing") {
        Position pos{.row = 2, .col = 2};
        PlaceNumberCommand cmd(game_state, validator, pos, 3);

        cmd.undo();
        REQUIRE(game_state->getCell(pos).value == 0);
    }

    SECTION("Execute and undo cycle") {
        Position pos{.row = 3, .col = 3};
        game_state->getCell(pos).value = 9;

        PlaceNumberCommand cmd(game_state, validator, pos, 4);

        cmd.execute();
        REQUIRE(game_state->getCell(pos).value == 4);

        cmd.undo();
        REQUIRE(game_state->getCell(pos).value == 9);
        REQUIRE(cmd.canUndo() == false);
    }

    SECTION("Valid state representation") {
        Position pos{.row = 4, .col = 4};
        PlaceNumberCommand cmd(game_state, validator, pos, 1);

        // Before execute, not a valid state
        REQUIRE(cmd.representsValidState() == false);

        cmd.execute();

        // After execute with valid board, should be valid
        REQUIRE(cmd.representsValidState() == true);
    }
}

TEST_CASE("RemoveNumberCommand - Edge Cases", "[game_commands][remove_number]") {
    auto game_state = std::make_shared<GameState>();
    auto validator = std::make_shared<GameValidator>();

    SECTION("Remove from cell with value") {
        Position pos{.row = 0, .col = 0};
        game_state->getCell(pos).value = 8;

        RemoveNumberCommand cmd(game_state, validator, pos);

        REQUIRE(cmd.canUndo() == false);

        cmd.execute();

        REQUIRE(game_state->getCell(pos).value == 0);
        REQUIRE(cmd.canUndo() == true);
        REQUIRE(cmd.getDescription() == "Remove number from (0,0)");
    }

    SECTION("Remove from empty cell") {
        Position pos{.row = 1, .col = 1};

        RemoveNumberCommand cmd(game_state, validator, pos);

        cmd.execute();
        REQUIRE(game_state->getCell(pos).value == 0);
    }

    SECTION("Double execute does nothing") {
        Position pos{.row = 2, .col = 2};
        game_state->getCell(pos).value = 6;

        RemoveNumberCommand cmd(game_state, validator, pos);

        cmd.execute();
        REQUIRE(game_state->getCell(pos).value == 0);

        cmd.execute();
        REQUIRE(game_state->getCell(pos).value == 0);
    }

    SECTION("Undo without execute does nothing") {
        Position pos{.row = 3, .col = 3};
        game_state->getCell(pos).value = 5;

        RemoveNumberCommand cmd(game_state, validator, pos);

        cmd.undo();
        REQUIRE(game_state->getCell(pos).value == 5);
    }

    SECTION("Execute and undo restores value") {
        Position pos{.row = 4, .col = 4};
        game_state->getCell(pos).value = 7;

        RemoveNumberCommand cmd(game_state, validator, pos);

        cmd.execute();
        REQUIRE(game_state->getCell(pos).value == 0);

        cmd.undo();
        REQUIRE(game_state->getCell(pos).value == 7);
    }

    SECTION("Valid state representation") {
        Position pos{.row = 5, .col = 5};
        RemoveNumberCommand cmd(game_state, validator, pos);

        REQUIRE(cmd.representsValidState() == false);

        cmd.execute();

        REQUIRE(cmd.representsValidState() == true);
    }
}

TEST_CASE("AddNoteCommand - Edge Cases", "[game_commands][add_note]") {
    auto game_state = std::make_shared<GameState>();

    SECTION("Add note to empty cell") {
        Position pos{.row = 0, .col = 0};
        AddNoteCommand cmd(game_state, pos, 5);

        REQUIRE(cmd.canUndo() == false);

        cmd.execute();

        const auto& notes = game_state->getCell(pos).notes;
        REQUIRE(notes.size() == 1);
        REQUIRE(notes[0] == 5);
        REQUIRE(cmd.canUndo() == true);
        REQUIRE(cmd.getDescription() == "Add note 5 to (0,0)");
        REQUIRE(cmd.representsValidState() == true);
    }

    SECTION("Add duplicate note") {
        Position pos{.row = 1, .col = 1};
        game_state->getCell(pos).notes = {3, 5, 7};

        AddNoteCommand cmd(game_state, pos, 5);

        cmd.execute();

        // Should still have same notes
        const auto& notes = game_state->getCell(pos).notes;
        REQUIRE(notes.size() == 3);
    }

    SECTION("Double execute does nothing") {
        Position pos{.row = 2, .col = 2};
        AddNoteCommand cmd(game_state, pos, 9);

        cmd.execute();
        cmd.execute();

        const auto& notes = game_state->getCell(pos).notes;
        REQUIRE(notes.size() == 1);
        REQUIRE(notes[0] == 9);
    }

    SECTION("Undo without execute does nothing") {
        Position pos{.row = 3, .col = 3};
        AddNoteCommand cmd(game_state, pos, 2);

        cmd.undo();

        const auto& notes = game_state->getCell(pos).notes;
        REQUIRE(notes.empty());
    }

    SECTION("Execute and undo removes note") {
        Position pos{.row = 4, .col = 4};
        AddNoteCommand cmd(game_state, pos, 8);

        cmd.execute();
        REQUIRE(game_state->getCell(pos).notes.size() == 1);

        cmd.undo();
        REQUIRE(game_state->getCell(pos).notes.empty());
    }

    SECTION("Undo duplicate note preserves it") {
        Position pos{.row = 5, .col = 5};
        game_state->getCell(pos).notes = {4};

        AddNoteCommand cmd(game_state, pos, 4);

        cmd.execute();
        REQUIRE(game_state->getCell(pos).notes.size() == 1);

        cmd.undo();
        // Should still have the original note
        REQUIRE(game_state->getCell(pos).notes.size() == 1);
        REQUIRE(game_state->getCell(pos).notes[0] == 4);
    }

    SECTION("Notes are sorted after adding") {
        Position pos{.row = 6, .col = 6};
        game_state->getCell(pos).notes = {2, 5};

        AddNoteCommand cmd(game_state, pos, 3);

        cmd.execute();

        const auto& notes = game_state->getCell(pos).notes;
        REQUIRE(notes.size() == 3);
        REQUIRE(notes[0] == 2);
        REQUIRE(notes[1] == 3);
        REQUIRE(notes[2] == 5);
    }
}

TEST_CASE("RemoveNoteCommand - Edge Cases", "[game_commands][remove_note]") {
    auto game_state = std::make_shared<GameState>();

    SECTION("Remove existing note") {
        Position pos{.row = 0, .col = 0};
        game_state->getCell(pos).notes = {1, 5, 9};

        RemoveNoteCommand cmd(game_state, pos, 5);

        REQUIRE(cmd.canUndo() == false);

        cmd.execute();

        const auto& notes = game_state->getCell(pos).notes;
        REQUIRE(notes.size() == 2);
        REQUIRE(std::find(notes.begin(), notes.end(), 5) == notes.end());
        REQUIRE(cmd.canUndo() == true);
        REQUIRE(cmd.getDescription() == "Remove note 5 from (0,0)");
        REQUIRE(cmd.representsValidState() == true);
    }

    SECTION("Remove non-existent note") {
        Position pos{.row = 1, .col = 1};
        game_state->getCell(pos).notes = {2, 4};

        RemoveNoteCommand cmd(game_state, pos, 7);

        cmd.execute();

        const auto& notes = game_state->getCell(pos).notes;
        REQUIRE(notes.size() == 2);
    }

    SECTION("Double execute does nothing") {
        Position pos{.row = 2, .col = 2};
        game_state->getCell(pos).notes = {3, 6};

        RemoveNoteCommand cmd(game_state, pos, 3);

        cmd.execute();
        cmd.execute();

        const auto& notes = game_state->getCell(pos).notes;
        REQUIRE(notes.size() == 1);
        REQUIRE(notes[0] == 6);
    }

    SECTION("Undo without execute does nothing") {
        Position pos{.row = 3, .col = 3};
        game_state->getCell(pos).notes = {1, 2, 3};

        RemoveNoteCommand cmd(game_state, pos, 2);

        cmd.undo();

        const auto& notes = game_state->getCell(pos).notes;
        REQUIRE(notes.size() == 3);
    }

    SECTION("Execute and undo restores note") {
        Position pos{.row = 4, .col = 4};
        game_state->getCell(pos).notes = {4, 8};

        RemoveNoteCommand cmd(game_state, pos, 4);

        cmd.execute();
        REQUIRE(game_state->getCell(pos).notes.size() == 1);

        cmd.undo();

        const auto& notes = game_state->getCell(pos).notes;
        REQUIRE(notes.size() == 2);
        REQUIRE(std::find(notes.begin(), notes.end(), 4) != notes.end());
    }

    SECTION("Undo non-existent note does nothing") {
        Position pos{.row = 5, .col = 5};
        game_state->getCell(pos).notes = {7};

        RemoveNoteCommand cmd(game_state, pos, 9);

        cmd.execute();
        cmd.undo();

        const auto& notes = game_state->getCell(pos).notes;
        REQUIRE(notes.size() == 1);
        REQUIRE(notes[0] == 7);
    }

    SECTION("Notes are sorted after undo") {
        Position pos{.row = 6, .col = 6};
        game_state->getCell(pos).notes = {2, 5, 8};

        RemoveNoteCommand cmd(game_state, pos, 5);

        cmd.execute();
        cmd.undo();

        const auto& notes = game_state->getCell(pos).notes;
        REQUIRE(notes.size() == 3);
        REQUIRE(notes[0] == 2);
        REQUIRE(notes[1] == 5);
        REQUIRE(notes[2] == 8);
    }
}

TEST_CASE("ClearNotesCommand - Edge Cases", "[game_commands][clear_notes]") {
    auto game_state = std::make_shared<GameState>();

    SECTION("Clear notes from cell with notes") {
        Position pos{.row = 0, .col = 0};
        game_state->getCell(pos).notes = {1, 3, 5, 7, 9};

        ClearNotesCommand cmd(game_state, pos);

        REQUIRE(cmd.canUndo() == false);

        cmd.execute();

        REQUIRE(game_state->getCell(pos).notes.empty());
        REQUIRE(cmd.canUndo() == true);
        REQUIRE(cmd.getDescription() == "Clear all notes from (0,0)");
        REQUIRE(cmd.representsValidState() == true);
    }

    SECTION("Clear notes from empty cell") {
        Position pos{.row = 1, .col = 1};

        ClearNotesCommand cmd(game_state, pos);

        cmd.execute();

        REQUIRE(game_state->getCell(pos).notes.empty());
    }

    SECTION("Double execute does nothing") {
        Position pos{.row = 2, .col = 2};
        game_state->getCell(pos).notes = {2, 4, 6};

        ClearNotesCommand cmd(game_state, pos);

        cmd.execute();
        cmd.execute();

        REQUIRE(game_state->getCell(pos).notes.empty());
    }

    SECTION("Undo without execute does nothing") {
        Position pos{.row = 3, .col = 3};
        game_state->getCell(pos).notes = {1, 2};

        ClearNotesCommand cmd(game_state, pos);

        cmd.undo();

        const auto& notes = game_state->getCell(pos).notes;
        REQUIRE(notes.size() == 2);
    }

    SECTION("Execute and undo restores all notes") {
        Position pos{.row = 4, .col = 4};
        game_state->getCell(pos).notes = {3, 6, 9};

        ClearNotesCommand cmd(game_state, pos);

        cmd.execute();
        REQUIRE(game_state->getCell(pos).notes.empty());

        cmd.undo();

        const auto& notes = game_state->getCell(pos).notes;
        REQUIRE(notes.size() == 3);
        REQUIRE(notes[0] == 3);
        REQUIRE(notes[1] == 6);
        REQUIRE(notes[2] == 9);
    }

    SECTION("Undo after clearing empty cell") {
        Position pos{.row = 5, .col = 5};

        ClearNotesCommand cmd(game_state, pos);

        cmd.execute();
        cmd.undo();

        REQUIRE(game_state->getCell(pos).notes.empty());
    }
}
