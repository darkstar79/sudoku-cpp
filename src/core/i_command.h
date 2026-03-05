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

#include <memory>
#include <string>

namespace sudoku::core {

/// Base class for implementing the Command pattern for undo/redo functionality
/// Uses Template Method pattern to eliminate boilerplate guard code
class ICommand {
public:
    virtual ~ICommand() = default;

    /// Execute the command (Template Method with guard)
    void execute() {
        if (executed_) {
            return;  // Already executed, prevent duplicate execution
        }
        executeImpl();
        executed_ = true;
    }

    /// Undo the command (Template Method with guard)
    void undo() {
        if (!executed_) {
            return;  // Not executed yet, nothing to undo
        }
        undoImpl();
        executed_ = false;
    }

    /// Check if this command can be undone
    [[nodiscard]] bool canUndo() const {
        return executed_;
    }

    /// Get a description of this command for debugging/logging
    [[nodiscard]] virtual std::string getDescription() const = 0;

    /// Check if this command represents a valid game state
    /// Used for "undo to last valid" functionality
    [[nodiscard]] virtual bool representsValidState() const = 0;

protected:
    /// Subclasses implement the actual execution logic
    virtual void executeImpl() = 0;

    /// Subclasses implement the actual undo logic
    virtual void undoImpl() = 0;

    /// Tracks whether this command has been executed
    bool executed_{false};

    // Protected special member functions to prevent slicing while allowing derived classes
    ICommand() = default;
    ICommand(const ICommand&) = default;
    ICommand& operator=(const ICommand&) = default;
    ICommand(ICommand&&) = default;
    ICommand& operator=(ICommand&&) = default;
};

/// Smart pointer type for commands
using CommandPtr = std::unique_ptr<ICommand>;

}  // namespace sudoku::core