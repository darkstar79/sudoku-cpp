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

#include "i_command.h"

#include <memory>
#include <vector>

namespace sudoku::core {

/// Manages command history for undo/redo functionality
class CommandHistory {
public:
    CommandHistory();
    ~CommandHistory() = default;

    // Default copy and move operations
    CommandHistory(const CommandHistory&) = default;
    CommandHistory& operator=(const CommandHistory&) = default;
    CommandHistory(CommandHistory&&) noexcept = default;
    CommandHistory& operator=(CommandHistory&&) noexcept = default;

    /// Execute a command and add it to the history
    void executeCommand(CommandPtr command);

    /// Undo the last command
    [[nodiscard]] bool undo();

    /// Redo the last undone command
    [[nodiscard]] bool redo();

    /// Undo to the last valid state (where representsValidState() returns true)
    [[nodiscard]] bool undoToLastValid();

    /// Check if undo is available
    [[nodiscard]] bool canUndo() const;

    /// Check if redo is available
    [[nodiscard]] bool canRedo() const;

    /// Check if there's a valid state to undo to
    [[nodiscard]] bool canUndoToValid() const;

    /// Clear all command history
    void clear();

    /// Get the number of commands that can be undone
    [[nodiscard]] size_t getUndoCount() const;

    /// Get the number of commands that can be redone
    [[nodiscard]] size_t getRedoCount() const;

    /// Get description of the command that would be undone
    [[nodiscard]] std::string getUndoDescription() const;

    /// Get description of the command that would be redone
    [[nodiscard]] std::string getRedoDescription() const;

    /// Set maximum history size (0 = unlimited)
    void setMaxHistorySize(size_t max_size);

    /// Get current history size
    [[nodiscard]] size_t getHistorySize() const;

private:
    std::vector<CommandPtr> commands_;
    size_t current_index_;  // Points to the next command to execute
    size_t max_history_size_;

    /// Remove old commands if history exceeds max size
    void trimHistory();

    /// Find the index of the last command that represents a valid state
    /// Returns current_index_ if no valid state found
    [[nodiscard]] size_t findLastValidStateIndex() const;
};

}  // namespace sudoku::core