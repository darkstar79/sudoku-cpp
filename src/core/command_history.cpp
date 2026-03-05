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

#include "command_history.h"

namespace sudoku::core {

CommandHistory::CommandHistory() : current_index_(0), max_history_size_(100) {  // Default to 100 commands
}

void CommandHistory::executeCommand(CommandPtr command) {
    if (!command) {
        return;
    }

    // Execute the command
    command->execute();

    // Remove any commands after current position (if we're in middle of history)
    if (current_index_ < commands_.size()) {
        commands_.erase(commands_.begin() + static_cast<std::vector<CommandPtr>::difference_type>(current_index_),
                        commands_.end());
    }

    // Add the new command
    commands_.push_back(std::move(command));
    current_index_ = commands_.size();

    // Trim history if necessary
    trimHistory();
}

bool CommandHistory::undo() {
    if (!canUndo()) {
        return false;
    }

    // Move back one position and undo that command
    --current_index_;
    commands_[current_index_]->undo();

    return true;
}

bool CommandHistory::redo() {
    if (!canRedo()) {
        return false;
    }

    // Execute the command at current position and move forward
    commands_[current_index_]->execute();
    ++current_index_;

    return true;
}

bool CommandHistory::undoToLastValid() {
    if (!canUndoToValid()) {
        return false;
    }

    size_t target_index = findLastValidStateIndex();

    // Undo commands until we reach the target index
    while (current_index_ > target_index) {
        if (!undo()) {
            break;
        }
    }

    return current_index_ == target_index;
}

bool CommandHistory::canUndo() const {
    return current_index_ > 0 && !commands_.empty();
}

bool CommandHistory::canRedo() const {
    return current_index_ < commands_.size();
}

bool CommandHistory::canUndoToValid() const {
    return findLastValidStateIndex() < current_index_;
}

void CommandHistory::clear() {
    commands_.clear();
    current_index_ = 0;
}

size_t CommandHistory::getUndoCount() const {
    return current_index_;
}

size_t CommandHistory::getRedoCount() const {
    return commands_.size() - current_index_;
}

std::string CommandHistory::getUndoDescription() const {
    if (!canUndo()) {
        return "";
    }
    return commands_[current_index_ - 1]->getDescription();
}

std::string CommandHistory::getRedoDescription() const {
    if (!canRedo()) {
        return "";
    }
    return commands_[current_index_]->getDescription();
}

void CommandHistory::setMaxHistorySize(size_t max_size) {
    max_history_size_ = max_size;
    trimHistory();
}

size_t CommandHistory::getHistorySize() const {
    return commands_.size();
}

void CommandHistory::trimHistory() {
    if (max_history_size_ == 0) {
        return;  // Unlimited
    }

    if (commands_.size() > max_history_size_) {
        size_t excess = commands_.size() - max_history_size_;

        // Remove commands from the beginning
        commands_.erase(commands_.begin(),
                        commands_.begin() + static_cast<std::vector<CommandPtr>::difference_type>(excess));

        // Adjust current index
        if (current_index_ >= excess) {
            current_index_ -= excess;
        } else {
            current_index_ = 0;
        }
    }
}

size_t CommandHistory::findLastValidStateIndex() const {
    // Start from current position and work backwards
    for (size_t i = current_index_; i > 0; --i) {
        size_t cmd_index = i - 1;
        if (commands_[cmd_index]->representsValidState()) {
            return i;  // Return the position after this command
        }
    }

    // No valid state found, return beginning
    return 0;
}

}  // namespace sudoku::core