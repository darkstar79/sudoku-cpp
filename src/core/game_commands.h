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
#include "i_command.h"
#include "i_game_validator.h"

#include <memory>

namespace sudoku::core {

/// Forward declaration
class IGameValidator;

/// Command to place a number on the board
class PlaceNumberCommand : public ICommand {
public:
    PlaceNumberCommand(std::shared_ptr<sudoku::model::GameState> game_state, std::shared_ptr<IGameValidator> validator,
                       const Position& position, int value);

    [[nodiscard]] std::string getDescription() const override;
    [[nodiscard]] bool representsValidState() const override;

protected:
    void executeImpl() override;
    void undoImpl() override;

private:
    std::shared_ptr<sudoku::model::GameState> game_state_;
    std::shared_ptr<IGameValidator> validator_;
    Position position_;
    int new_value_{0};
    int previous_value_{0};
    bool was_valid_before_{false};
    bool is_valid_after_{false};
};

/// Command to remove a number from the board
class RemoveNumberCommand : public ICommand {
public:
    RemoveNumberCommand(std::shared_ptr<sudoku::model::GameState> game_state, std::shared_ptr<IGameValidator> validator,
                        const Position& position);

    [[nodiscard]] std::string getDescription() const override;
    [[nodiscard]] bool representsValidState() const override;

protected:
    void executeImpl() override;
    void undoImpl() override;

private:
    std::shared_ptr<sudoku::model::GameState> game_state_;
    std::shared_ptr<IGameValidator> validator_;
    Position position_;
    int previous_value_{0};
    bool was_valid_before_{false};
    bool is_valid_after_{false};
};

/// Command to add a note to a cell
class AddNoteCommand : public ICommand {
public:
    AddNoteCommand(std::shared_ptr<sudoku::model::GameState> game_state, const Position& position, int note_value);

    [[nodiscard]] std::string getDescription() const override;
    [[nodiscard]] bool representsValidState() const override;

protected:
    void executeImpl() override;
    void undoImpl() override;

private:
    std::shared_ptr<sudoku::model::GameState> game_state_;
    Position position_;
    int note_value_{0};
    bool was_note_present_{false};
};

/// Command to remove a note from a cell
class RemoveNoteCommand : public ICommand {
public:
    RemoveNoteCommand(std::shared_ptr<sudoku::model::GameState> game_state, const Position& position, int note_value);

    [[nodiscard]] std::string getDescription() const override;
    [[nodiscard]] bool representsValidState() const override;

protected:
    void executeImpl() override;
    void undoImpl() override;

private:
    std::shared_ptr<sudoku::model::GameState> game_state_;
    Position position_;
    int note_value_{0};
    bool was_note_present_{false};
};

/// Command to clear all notes from a cell
class ClearNotesCommand : public ICommand {
public:
    ClearNotesCommand(std::shared_ptr<sudoku::model::GameState> game_state, const Position& position);

    [[nodiscard]] std::string getDescription() const override;
    [[nodiscard]] bool representsValidState() const override;

protected:
    void executeImpl() override;
    void undoImpl() override;

private:
    std::shared_ptr<sudoku::model::GameState> game_state_;
    Position position_;
    std::vector<int> previous_notes_;
};

}  // namespace sudoku::core