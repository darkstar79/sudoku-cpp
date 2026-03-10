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

#include "game_state.h"

#include "core/board_utils.h"
#include "core/constants.h"
#include "core/i_game_validator.h"
#include "core/i_puzzle_generator.h"
#include "core/i_time_provider.h"
#include "core/observable.h"

#include <algorithm>
#include <functional>
#include <ranges>
#include <utility>

namespace sudoku::model {

GameState::GameState(std::shared_ptr<core::ITimeProvider> time_provider)
    : onBoardChanged(false), onMoveCountChanged(0), onGameCompleted(false),
      onDifficultyChanged(core::Difficulty::Medium), time_provider_(std::move(time_provider)) {
    initializeBoard();
}

void GameState::initializeBoard() {
    board_.resize(9);
    for (auto& row : board_) {
        row.resize(9);
    }
}

std::chrono::milliseconds GameState::getElapsedTime() const {
    if (timer_running_) {
        auto now = time_provider_->systemNow();
        auto current_session = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
        return elapsed_time_ + current_session;
    }
    return elapsed_time_;
}

void GameState::startTimer() {
    start_time_ = time_provider_->systemNow();
    timer_running_ = true;
    markDirty();
}

void GameState::pauseTimer() {
    if (timer_running_) {
        pause_time_ = time_provider_->systemNow();
        elapsed_time_ += std::chrono::duration_cast<std::chrono::milliseconds>(pause_time_ - start_time_);
        timer_running_ = false;
        markDirty();
    }
}

void GameState::resumeTimer() {
    if (!timer_running_) {
        start_time_ = time_provider_->systemNow();
        timer_running_ = true;
        markDirty();
    }
}

void GameState::resetTimer() {
    elapsed_time_ = std::chrono::milliseconds{0};
    timer_running_ = false;
    markDirty();
}

void GameState::setSolutionBoard(const std::vector<std::vector<int>>& solution) {
    solution_board_ = solution;
    markDirty();
}

void GameState::setPuzzleSeed(uint32_t seed) {
    puzzle_seed_ = seed;
    markDirty();
}

void GameState::setSelectedPosition(const core::Position& pos) {
    selected_position_ = pos;
    markDirty();
}

void GameState::clearSelection() {
    selected_position_ = std::nullopt;
    markDirty();
}

void GameState::clearBoard() {
    core::forEachCell([&](size_t row, size_t col) {
        board_[row][col] = Cell{};  // Reset to default state
    });

    // Reset game state
    is_complete_ = false;
    move_count_ = 0;
    mistake_count_ = 0;
    selected_position_ = std::nullopt;
    resetTimer();

    // Notify observers
    onBoardChanged.set(true);
    onMoveCountChanged.set(move_count_);
    onGameCompleted.set(is_complete_);
    markDirty();
}

void GameState::setDifficulty(core::Difficulty difficulty) {
    if (difficulty_ != difficulty) {
        difficulty_ = difficulty;
        onDifficultyChanged.set(difficulty_);
        markDirty();
    }
}

void GameState::setComplete(bool complete) {
    if (is_complete_ != complete) {
        is_complete_ = complete;
        onGameCompleted.set(is_complete_);
        markDirty();
    }
}

void GameState::incrementMoves() {
    ++move_count_;
    onMoveCountChanged.set(move_count_);
    markDirty();
}

void GameState::incrementMistakes() {
    ++mistake_count_;
    markDirty();
}

void GameState::loadPuzzle(const std::vector<std::vector<int>>& puzzle) {
    clearBoard();

    core::forEachCell([&](size_t row, size_t col) {
        if (puzzle[row][col] != 0) {
            board_[row][col].value = puzzle[row][col];
            board_[row][col].is_given = true;
        }
    });

    // Notify that board has changed
    onBoardChanged.set(true);
    markDirty();
}

std::vector<std::vector<int>> GameState::extractNumbers() const {
    std::vector<std::vector<int>> numbers(core::BOARD_SIZE, std::vector<int>(core::BOARD_SIZE, 0));

    core::forEachCell([&](size_t row, size_t col) { numbers[row][col] = board_[row][col].value; });

    return numbers;
}

std::vector<std::vector<int>> GameState::extractGivenNumbers() const {
    std::vector<std::vector<int>> given(core::BOARD_SIZE, std::vector<int>(core::BOARD_SIZE, 0));

    core::forEachCell([&](size_t row, size_t col) {
        if (board_[row][col].is_given) {
            given[row][col] = board_[row][col].value;
        }
    });

    return given;
}

void GameState::updateConflicts(const std::vector<core::Position>& conflicts) {
    // Clear existing conflicts
    clearConflicts();

    // Set new conflicts
    for (const auto& pos : conflicts) {
        if (pos.row < core::BOARD_SIZE && pos.col < core::BOARD_SIZE) {
            board_[pos.row][pos.col].has_conflict = true;
        }
    }
    markDirty();
}

void GameState::clearConflicts() {
    core::forEachCell([&](size_t row, size_t col) { board_[row][col].has_conflict = false; });
    markDirty();
}

std::vector<core::Position> GameState::getConflictPositions() const {
    std::vector<core::Position> conflicts;

    core::forEachCell([&](size_t row, size_t col) {
        if (board_[row][col].has_conflict) {
            conflicts.push_back({.row = row, .col = col});
        }
    });

    return conflicts;
}

void GameState::addNote(const core::Position& pos, int value) {
    if (pos.row < core::BOARD_SIZE && pos.col < core::BOARD_SIZE && value >= core::MIN_VALUE &&
        value <= core::MAX_VALUE) {
        auto& notes = board_[pos.row][pos.col].notes;
        if (!std::ranges::contains(notes, value)) {
            notes.push_back(value);
            std::ranges::sort(notes);
            markDirty();
        }
    }
}

void GameState::removeNote(const core::Position& pos, int value) {
    if (pos.row < core::BOARD_SIZE && pos.col < core::BOARD_SIZE) {
        auto& notes = board_[pos.row][pos.col].notes;
        auto original_size = notes.size();
        std::erase(notes, value);
        if (notes.size() != original_size) {
            markDirty();
        }
    }
}

void GameState::clearNotes(const core::Position& pos) {
    if (pos.row < core::BOARD_SIZE && pos.col < core::BOARD_SIZE) {
        auto& notes = board_[pos.row][pos.col].notes;
        if (!notes.empty()) {
            notes.clear();
            markDirty();
        }
    }
}

void GameState::toggleNote(const core::Position& pos, int value) {
    if (pos.row < core::BOARD_SIZE && pos.col < core::BOARD_SIZE && value >= core::MIN_VALUE &&
        value <= core::MAX_VALUE) {
        auto& notes = board_[pos.row][pos.col].notes;
        auto it = std::ranges::find(notes, value);
        if (it != notes.end()) {
            notes.erase(it);
        } else {
            notes.push_back(value);
            std::ranges::sort(notes);
        }
        markDirty();
    }
}

void GameState::highlightNumber(int number) {
    clearHighlights();
    if (number >= 1 && number <= 9) {
        core::forEachCell([&](size_t row, size_t col) {
            if (board_[row][col].value == number) {
                board_[row][col].is_highlighted = true;
            }
        });
        markDirty();
    }
}

void GameState::clearHighlights() {
    core::forEachCell([&](size_t row, size_t col) { board_[row][col].is_highlighted = false; });
    markDirty();
}

void GameState::highlightRelated(const core::Position& pos) {
    clearHighlights();

    if (pos.row >= core::BOARD_SIZE || pos.col >= core::BOARD_SIZE) {
        return;
    }

    // Highlight same row
    for (size_t col = 0; col < core::BOARD_SIZE; ++col) {
        board_[pos.row][col].is_highlighted = true;
    }

    // Highlight same column
    for (size_t row = 0; row < core::BOARD_SIZE; ++row) {
        board_[row][pos.col].is_highlighted = true;
    }

    // Highlight same 3x3 box
    size_t box_start_row = (pos.row / core::BOX_SIZE) * core::BOX_SIZE;
    size_t box_start_col = (pos.col / core::BOX_SIZE) * core::BOX_SIZE;
    core::forEachBoxCell(box_start_row, box_start_col,
                         [&](size_t row, size_t col) { board_[row][col].is_highlighted = true; });
    markDirty();
}

void GameState::markCellAsHintRevealed(const core::Position& pos) {
    if (pos.row < core::BOARD_SIZE && pos.col < core::BOARD_SIZE) {
        board_[pos.row][pos.col].is_hint_revealed = true;
        markDirty();
    }
}

bool GameState::isCellHintRevealed(const core::Position& pos) const {
    if (pos.row < core::BOARD_SIZE && pos.col < core::BOARD_SIZE) {
        return board_[pos.row][pos.col].is_hint_revealed;
    }
    return false;
}

bool GameState::operator==(const GameState& other) const {
    // For Observable pattern, compare key state that UI cares about
    // NOTE: timer_running_ MUST be included - it determines isGameActive() which gates all input
    return board_ == other.board_ && difficulty_ == other.difficulty_ && is_complete_ == other.is_complete_ &&
           move_count_ == other.move_count_ && mistake_count_ == other.mistake_count_ &&
           selected_position_ == other.selected_position_ && timer_running_ == other.timer_running_;
}

}  // namespace sudoku::model