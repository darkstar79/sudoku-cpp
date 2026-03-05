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

#include "../core/string_keys.h"
#include "core/constants.h"
#include "main_window.h"

#include <string>

#include <imgui.h>
#include <spdlog/spdlog.h>

namespace sudoku::view {

using namespace core::StringKeys;

void MainWindow::renderGameBoard() {
    ImGui::SetNextWindowPos(ImVec2(0, 100), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y - 150),
                             ImGuiCond_Always);

    ImGui::Begin("Game Board", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse);

    if (!view_model_) {
        ImGui::SetCursorPosY((ImGui::GetWindowHeight() * 0.5f) - 10);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() * 0.5f) - 100);
        ImGui::Text("%s", loc(BoardNoGameLoaded));
        ImGui::End();
        return;
    }

    const auto& game_state = view_model_->gameState.get();

    // Calculate board dimensions
    ImVec2 window_size = ImGui::GetWindowSize();
    float board_size = std::min(window_size.x - 40, window_size.y - 120);
    board_size = std::min(board_size, UIConstants::BOARD_MAX_SIZE);
    board_size = std::max(board_size, UIConstants::BOARD_MIN_SIZE);
    float cell_size = board_size / static_cast<float>(sudoku::core::BOARD_SIZE);

    ImVec2 board_pos((window_size.x - board_size) * 0.5f, 20.0f);

    ImGui::SetCursorPos(board_pos);
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Render board components
    renderBoardBackground(draw_list, canvas_pos, board_size);

    auto selected_pos_opt = game_state.getSelectedPosition();

    // Render all cells
    for (size_t row = 0; row < sudoku::core::BOARD_SIZE; ++row) {
        for (size_t col = 0; col < sudoku::core::BOARD_SIZE; ++col) {
            ImVec2 cell_pos(canvas_pos.x + (static_cast<float>(col) * cell_size),
                            canvas_pos.y + (static_cast<float>(row) * cell_size));
            const auto& cell = game_state.getBoard()[row][col];
            bool is_selected =
                selected_pos_opt.has_value() && (selected_pos_opt->row == row && selected_pos_opt->col == col);

            renderSingleCell(draw_list, cell, row, col, cell_pos, cell_size, is_selected);
        }
    }

    renderBoardGridLines(draw_list, canvas_pos, board_size, cell_size);
    renderActionButtons(board_pos, board_size);

    ImGui::End();
}

void MainWindow::renderBoardBackground(ImDrawList* draw_list, const ImVec2& canvas_pos, float board_size) {
    // Draw border
    draw_list->AddRectFilled(ImVec2(canvas_pos.x - 2, canvas_pos.y - 2),
                             ImVec2(canvas_pos.x + board_size + 2, canvas_pos.y + board_size + 2),
                             UIConstants::Colors::BOARD_BORDER);

    // Draw white background
    draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + board_size, canvas_pos.y + board_size),
                             UIConstants::Colors::BOARD_BACKGROUND);
}

void MainWindow::renderSingleCell(ImDrawList* draw_list, const model::Cell& cell, size_t row, size_t col,
                                  const ImVec2& cell_pos, float cell_size, bool is_selected) {
    // Draw cell background
    ImU32 cell_color = is_selected ? UIConstants::Colors::CELL_SELECTED : UIConstants::Colors::CELL_BACKGROUND;
    draw_list->AddRectFilled(cell_pos, ImVec2(cell_pos.x + cell_size, cell_pos.y + cell_size), cell_color);

    // Draw selection outline
    if (is_selected) {
        draw_list->AddRect(ImVec2(cell_pos.x + 2, cell_pos.y + 2),
                           ImVec2(cell_pos.x + cell_size - 2, cell_pos.y + cell_size - 2),
                           UIConstants::Colors::CELL_SELECTION_OUTLINE, 0.0f, 0, 3.0f  // 3px outline
        );
    }

    // Draw cell content
    if (cell.value > 0) {
        renderCellValue(draw_list, cell, cell_pos, cell_size);
    } else if (!cell.notes.empty()) {
        renderCellNotes(draw_list, cell, cell_pos, cell_size);
    }

    // Handle cell clicks
    ImGui::SetCursorScreenPos(cell_pos);
    ImGui::InvisibleButton(("cell_" + std::to_string(row) + "_" + std::to_string(col)).c_str(),
                           ImVec2(cell_size, cell_size));

    if (ImGui::IsItemClicked()) {
        handleCellClick(static_cast<int>(row), static_cast<int>(col));
    }
}

void MainWindow::renderCellValue(ImDrawList* draw_list, const model::Cell& cell, const ImVec2& cell_pos,
                                 float cell_size) {
    std::string number_str(1, static_cast<char>('0' + cell.value));

    ImGui::PushFont(font_manager_->getFont(FontType::Large));
    ImVec2 text_size = ImGui::CalcTextSize(number_str.c_str());
    ImVec2 text_pos(cell_pos.x + ((cell_size - text_size.x) * 0.5f), cell_pos.y + ((cell_size - text_size.y) * 0.5f));

    // Determine text color
    ImU32 text_color = UIConstants::Colors::TEXT_GIVEN;  // Default: given numbers
    if (cell.is_hint_revealed) {
        text_color = UIConstants::Colors::TEXT_HINT;  // Hint-revealed (orange)
    } else if (!cell.is_given) {
        text_color = cell.has_conflict ? UIConstants::Colors::TEXT_ERROR :  // Error red
                         UIConstants::Colors::TEXT_USER;                    // User numbers (blue)
    }

    draw_list->AddText(text_pos, text_color, number_str.c_str());
    ImGui::PopFont();
}

void MainWindow::renderCellNotes(ImDrawList* draw_list, const model::Cell& cell, const ImVec2& cell_pos,
                                 float cell_size) {
    ImGui::PushFont(font_manager_->getFont(FontType::Medium));

    for (int note : cell.notes) {
        if (note >= sudoku::core::MIN_VALUE && note <= sudoku::core::MAX_VALUE) {
            int note_row = (note - 1) / static_cast<int>(sudoku::core::BOX_SIZE);
            int note_col = (note - 1) % static_cast<int>(sudoku::core::BOX_SIZE);

            std::string note_str(1, static_cast<char>('0' + note));
            ImVec2 note_text_size = ImGui::CalcTextSize(note_str.c_str());

            // Position notes in 3x3 grid within cell
            float note_cell_width = cell_size / static_cast<float>(sudoku::core::BOX_SIZE);
            float note_cell_height = cell_size / static_cast<float>(sudoku::core::BOX_SIZE);

            ImVec2 note_pos(cell_pos.x + (static_cast<float>(note_col) * note_cell_width) +
                                ((note_cell_width - note_text_size.x) * 0.5f),
                            cell_pos.y + (static_cast<float>(note_row) * note_cell_height) +
                                ((note_cell_height - note_text_size.y) * 0.5f));

            draw_list->AddText(note_pos, UIConstants::Colors::TEXT_NOTE, note_str.c_str());
        }
    }

    ImGui::PopFont();
}

void MainWindow::renderBoardGridLines(ImDrawList* draw_list, const ImVec2& canvas_pos, float board_size,
                                      float cell_size) {
    for (size_t i = 1; i < sudoku::core::BOARD_SIZE; ++i) {
        bool is_box_border = (i % sudoku::core::BOX_SIZE == 0);
        float thickness = is_box_border ? 2.0f : 1.0f;
        ImU32 color = is_box_border ? UIConstants::Colors::GRID_THICK_LINE : UIConstants::Colors::GRID_THIN_LINE;

        // Vertical lines
        draw_list->AddLine(ImVec2(canvas_pos.x + (static_cast<float>(i) * cell_size), canvas_pos.y),
                           ImVec2(canvas_pos.x + (static_cast<float>(i) * cell_size), canvas_pos.y + board_size), color,
                           thickness);

        // Horizontal lines
        draw_list->AddLine(ImVec2(canvas_pos.x, canvas_pos.y + (static_cast<float>(i) * cell_size)),
                           ImVec2(canvas_pos.x + board_size, canvas_pos.y + (static_cast<float>(i) * cell_size)), color,
                           thickness);
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — renders all action button rows with state-dependent enable/disable; branching is inherent to UI rendering
void MainWindow::renderActionButtons(const ImVec2& board_pos, float board_size) {
    ImGui::SetCursorPos(ImVec2(board_pos.x, board_pos.y + board_size + 30));

    float total_button_width = (130 * 6) + (10 * 5);  // 6 buttons with spacing
    float button_start_x = board_pos.x + ((board_size - total_button_width) * 0.5f);

    ImGui::SetCursorPosX(button_start_x);

    // Style buttons
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.96f, 0.96f, 0.96f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.88f, 0.88f, 0.88f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    if (ImGui::Button(loc(ButtonCheckSolution), ImVec2(150, 32))) {
        if (view_model_) {
            view_model_->checkSolution();
        }
    }

    ImGui::SameLine();

    bool can_reset = view_model_ && view_model_->canExecuteCommand(viewmodel::GameCommand::ResetGame);
    if (!can_reset) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(loc(ButtonResetPuzzle), ImVec2(150, 32))) {
        show_reset_dialog_ = true;
    }
    if (!can_reset) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    bool can_undo = view_model_ && view_model_->canUndo();
    if (!can_undo) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(loc(ButtonUndo), ImVec2(150, 32))) {
        if (view_model_) {
            view_model_->undo();
        }
    }
    if (!can_undo) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    bool can_redo = view_model_ && view_model_->canRedo();
    if (!can_redo) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(loc(ButtonRedo), ImVec2(150, 32))) {
        if (view_model_) {
            view_model_->redo();
        }
    }
    if (!can_redo) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    if (ImGui::Button(loc(ButtonUndoToValid), ImVec2(150, 32))) {
        if (view_model_) {
            view_model_->undoToLastValid();
        }
    }

    ImGui::SameLine();

    // Auto Notes toggle button with active-state highlighting
    bool auto_notes_active = view_model_ && view_model_->isAutoNotesEnabled();
    bool game_active = view_model_ && view_model_->isGameActive();
    if (!game_active) {
        ImGui::BeginDisabled();
    }
    if (auto_notes_active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.145F, 0.388F, 0.922F, 1.0F));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2F, 0.44F, 0.96F, 1.0F));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0F, 1.0F, 1.0F, 1.0F));
    }
    if (ImGui::Button(auto_notes_active ? loc(ButtonAutoNotesOn) : loc(ButtonAutoNotesOff), ImVec2(150, 32))) {
        if (view_model_) {
            view_model_->toggleAutoNotes();
        }
    }
    if (auto_notes_active) {
        ImGui::PopStyleColor(3);
    }
    if (!game_active) {
        ImGui::EndDisabled();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
}

}  // namespace sudoku::view
