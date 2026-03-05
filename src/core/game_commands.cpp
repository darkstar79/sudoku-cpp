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

#include "game_commands.h"

#include "game_validator.h"

#include <algorithm>
#include <ranges>

#include <fmt/format.h>

namespace sudoku::core {

// PlaceNumberCommand Implementation
PlaceNumberCommand::PlaceNumberCommand(std::shared_ptr<sudoku::model::GameState> game_state,
                                       std::shared_ptr<IGameValidator> validator, const Position& position, int value)
    : game_state_(std::move(game_state)), validator_(std::move(validator)), position_(position), new_value_(value) {
}

void PlaceNumberCommand::executeImpl() {
    // Store previous state
    previous_value_ = game_state_->getCell(position_).value;
    was_valid_before_ = validator_->validateBoard(game_state_->extractNumbers());

    // Execute the command
    game_state_->getCell(position_).value = new_value_;
    game_state_->incrementMoves();

    // Check if result is valid
    is_valid_after_ = validator_->validateBoard(game_state_->extractNumbers());
}

void PlaceNumberCommand::undoImpl() {
    // Restore previous state
    game_state_->getCell(position_).value = previous_value_;
}

std::string PlaceNumberCommand::getDescription() const {
    return fmt::format("Place {} at ({},{})", new_value_, position_.row, position_.col);
}

bool PlaceNumberCommand::representsValidState() const {
    return executed_ && is_valid_after_;
}

// RemoveNumberCommand Implementation
RemoveNumberCommand::RemoveNumberCommand(std::shared_ptr<sudoku::model::GameState> game_state,
                                         std::shared_ptr<IGameValidator> validator, const Position& position)
    : game_state_(std::move(game_state)), validator_(std::move(validator)), position_(position) {
}

void RemoveNumberCommand::executeImpl() {
    // Store previous state
    previous_value_ = game_state_->getCell(position_).value;
    was_valid_before_ = validator_->validateBoard(game_state_->extractNumbers());

    // Execute the command
    game_state_->getCell(position_).value = 0;
    game_state_->incrementMoves();

    // Check if result is valid
    is_valid_after_ = validator_->validateBoard(game_state_->extractNumbers());
}

void RemoveNumberCommand::undoImpl() {
    // Restore previous state
    game_state_->getCell(position_).value = previous_value_;
}

std::string RemoveNumberCommand::getDescription() const {
    return fmt::format("Remove number from ({},{})", position_.row, position_.col);
}

bool RemoveNumberCommand::representsValidState() const {
    return executed_ && is_valid_after_;
}

// AddNoteCommand Implementation
AddNoteCommand::AddNoteCommand(std::shared_ptr<sudoku::model::GameState> game_state, const Position& position,
                               int note_value)
    : game_state_(std::move(game_state)), position_(position), note_value_(note_value) {
}

void AddNoteCommand::executeImpl() {
    auto& cell = game_state_->getCell(position_);
    auto& notes = cell.notes;

    // Check if note was already present
    was_note_present_ = std::ranges::find(notes, note_value_) != notes.end();

    if (!was_note_present_) {
        notes.push_back(note_value_);
        std::ranges::sort(notes);
    }
}

void AddNoteCommand::undoImpl() {
    if (!was_note_present_) {
        auto& notes = game_state_->getCell(position_).notes;
        std::erase(notes, note_value_);
    }
}

std::string AddNoteCommand::getDescription() const {
    return fmt::format("Add note {} to ({},{})", note_value_, position_.row, position_.col);
}

bool AddNoteCommand::representsValidState() const {
    // Note operations don't affect board validity
    return true;
}

// RemoveNoteCommand Implementation
RemoveNoteCommand::RemoveNoteCommand(std::shared_ptr<sudoku::model::GameState> game_state, const Position& position,
                                     int note_value)
    : game_state_(std::move(game_state)), position_(position), note_value_(note_value) {
}

void RemoveNoteCommand::executeImpl() {
    auto& cell = game_state_->getCell(position_);
    auto& notes = cell.notes;

    // Check if note was present
    was_note_present_ = std::ranges::find(notes, note_value_) != notes.end();

    if (was_note_present_) {
        std::erase(notes, note_value_);
    }
}

void RemoveNoteCommand::undoImpl() {
    if (was_note_present_) {
        auto& notes = game_state_->getCell(position_).notes;
        notes.push_back(note_value_);
        std::ranges::sort(notes);
    }
}

std::string RemoveNoteCommand::getDescription() const {
    return fmt::format("Remove note {} from ({},{})", note_value_, position_.row, position_.col);
}

bool RemoveNoteCommand::representsValidState() const {
    // Note operations don't affect board validity
    return true;
}

// ClearNotesCommand Implementation
ClearNotesCommand::ClearNotesCommand(std::shared_ptr<sudoku::model::GameState> game_state, const Position& position)
    : game_state_(std::move(game_state)), position_(position) {
}

void ClearNotesCommand::executeImpl() {
    auto& cell = game_state_->getCell(position_);

    // Store previous notes
    previous_notes_ = cell.notes;

    // Clear all notes
    cell.notes.clear();
}

void ClearNotesCommand::undoImpl() {
    // Restore previous notes
    game_state_->getCell(position_).notes = previous_notes_;
}

std::string ClearNotesCommand::getDescription() const {
    return fmt::format("Clear all notes from ({},{})", position_.row, position_.col);
}

bool ClearNotesCommand::representsValidState() const {
    // Note operations don't affect board validity
    return true;
}

}  // namespace sudoku::core