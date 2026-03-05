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

#include "core/constants.h"
#include "main_window.h"

#include <chrono>

#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>

namespace sudoku::view {

void MainWindow::handleCellClick(int row, int col) {
    if (view_model_) {
        view_model_->selectCell(row, col);
    }
}

void MainWindow::handleKeyboardInput(int scancode) {
    if (!view_model_) {
        return;
    }

    // Handle number keys (standard and keypad)
    if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) {
        handleNumberInput(scancode - SDL_SCANCODE_1 + 1);
        return;
    }
    if (scancode >= SDL_SCANCODE_KP_1 && scancode <= SDL_SCANCODE_KP_9) {
        handleNumberInput(scancode - SDL_SCANCODE_KP_1 + 1);
        return;
    }

    // Handle editing keys (clear, delete, etc.)
    handleEditingInput(scancode);

    // Handle modified keys (Ctrl+Z, Ctrl+Y, etc.)
    handleModifiedKeyInput(scancode);

    // Handle function keys (F1, etc.)
    handleFunctionKeyInput(scancode);

    // Handle navigation keys (requires current position)
    const auto& game_state = view_model_->gameState.get();
    auto current_pos_opt = game_state.getSelectedPosition();
    if (!current_pos_opt.has_value()) {
        return;  // No selection, nothing to navigate
    }
    handleNavigationInput(scancode, current_pos_opt.value());
}

void MainWindow::handleNumberInput(int number) {
    if (!view_model_) {
        return;
    }

    const auto& game_state = view_model_->gameState.get();
    if (!game_state.isTimerRunning()) {
        return;  // No game active
    }

    auto current_pos_opt = game_state.getSelectedPosition();
    if (!current_pos_opt.has_value()) {
        return;  // No cell selected
    }

    const auto& current_pos = current_pos_opt.value();
    const auto& cell = game_state.getCell(current_pos.row, current_pos.col);

    // Don't allow editing given numbers
    if (cell.is_given) {
        return;
    }

    // Auto-notes mode: single press directly places/toggles number (no double-press needed)
    if (view_model_->isAutoNotesEnabled()) {
        if (cell.value == 0) {
            view_model_->enterNumber(number);
        } else if (cell.value == number) {
            view_model_->clearSelectedCell();
        }
        return;
    }

    // Manual notes mode: single press toggles note, double press places number
    auto current_time = std::chrono::steady_clock::now();
    bool is_double_press =
        (number == last_number_pressed_ && current_time - last_press_time_ < double_press_threshold_);

    if (is_double_press) {
        view_model_->enterNumber(number);

        // Reset double-press detection
        last_number_pressed_ = 0;
        last_press_time_ = {};
    } else {
        if (cell.value == 0) {
            view_model_->enterNote(number);
        } else if (cell.value == number && !cell.is_given) {
            view_model_->clearSelectedCell();
        }

        // Update double-press detection
        last_number_pressed_ = number;
        last_press_time_ = current_time;
    }
}

void MainWindow::handleNavigationInput(int scancode, const core::Position& current_pos) {
    switch (scancode) {
        case SDL_SCANCODE_UP:
            if (current_pos.row > 0) {
                view_model_->selectCell(current_pos.row - 1, current_pos.col);
            } else {
                view_model_->selectCell(core::BOARD_SIZE - 1, current_pos.col);  // Wrap to bottom
            }
            break;

        case SDL_SCANCODE_DOWN:
            if (current_pos.row < core::BOARD_SIZE - 1) {
                view_model_->selectCell(current_pos.row + 1, current_pos.col);
            } else {
                view_model_->selectCell(0, current_pos.col);  // Wrap to top
            }
            break;

        case SDL_SCANCODE_LEFT:
            if (current_pos.col > 0) {
                view_model_->selectCell(current_pos.row, current_pos.col - 1);
            } else {
                view_model_->selectCell(current_pos.row, core::BOARD_SIZE - 1);  // Wrap to right
            }
            break;

        case SDL_SCANCODE_RIGHT:
            if (current_pos.col < core::BOARD_SIZE - 1) {
                view_model_->selectCell(current_pos.row, current_pos.col + 1);
            } else {
                view_model_->selectCell(current_pos.row, 0);  // Wrap to left
            }
            break;

        case SDL_SCANCODE_TAB: {
            SDL_Keymod mod = SDL_GetModState();
            if (mod & SDL_KMOD_SHIFT) {
                // Shift+Tab - move backwards
                int new_col = static_cast<int>(current_pos.col) - 1;
                int new_row = static_cast<int>(current_pos.row);
                if (new_col < 0) {
                    new_col = static_cast<int>(core::BOARD_SIZE) - 1;
                    new_row = (new_row - 1 + static_cast<int>(core::BOARD_SIZE)) % static_cast<int>(core::BOARD_SIZE);
                }
                view_model_->selectCell(new_row, new_col);
            } else {
                // Tab - move forwards
                size_t new_col = current_pos.col + 1;
                size_t new_row = current_pos.row;
                if (new_col > core::BOARD_SIZE - 1) {
                    new_col = 0;
                    new_row = (new_row + 1) % core::BOARD_SIZE;
                }
                view_model_->selectCell(new_row, new_col);
            }
            break;
        }

        default:
            break;
    }
}

void MainWindow::handleModifiedKeyInput(int scancode) {
    SDL_Keymod mod = SDL_GetModState();

    switch (scancode) {
        case SDL_SCANCODE_Z:
            if (mod & SDL_KMOD_CTRL) {
                if (mod & SDL_KMOD_SHIFT) {
                    view_model_->undoToLastValid();
                } else {
                    view_model_->undo();
                }
            }
            break;

        case SDL_SCANCODE_Y:
            if (mod & SDL_KMOD_CTRL) {
                view_model_->redo();
            }
            break;

        case SDL_SCANCODE_R:
            if (mod & SDL_KMOD_CTRL) {
                if (view_model_->isGameActive()) {
                    show_reset_dialog_ = true;
                }
            }
            break;

        default:
            break;
    }
}

void MainWindow::handleEditingInput(int scancode) {
    switch (scancode) {
        case SDL_SCANCODE_DELETE:
        case SDL_SCANCODE_BACKSPACE:
        case SDL_SCANCODE_0:
        case SDL_SCANCODE_KP_0:
            view_model_->clearSelectedCell();
            break;

        case SDL_SCANCODE_H:
            view_model_->getHint();
            break;

        default:
            break;
    }
}

void MainWindow::handleFunctionKeyInput(int scancode) {
    switch (scancode) {
        case SDL_SCANCODE_F1:
            menu_visible_ = !menu_visible_;
            spdlog::debug("Menu visibility toggled: {}", menu_visible_ ? "visible" : "hidden");
            break;

        default:
            break;
    }
}

}  // namespace sudoku::view
