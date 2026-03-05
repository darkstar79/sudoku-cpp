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

#include "../core/puzzle_rating.h"
#include "../core/string_keys.h"
#include "main_window.h"

#include <array>
#include <limits>
#include <string>
#include <utility>

#include <imgui.h>
#include <spdlog/spdlog.h>

namespace sudoku::view {

using namespace core::StringKeys;

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — renders all menu items across File/Game/Help menus; each branch is a distinct UI action; cannot split without redundant state threading
void MainWindow::renderMenuBar() {
    if (!menu_visible_) {
        return;  // Early return when menu is hidden
    }

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu(loc(MenuGame))) {
            if (ImGui::MenuItem(loc(MenuNewGame), "Ctrl+N")) {
                show_new_game_dialog_ = true;
            }

            if (ImGui::MenuItem(loc(MenuResetPuzzle), "Ctrl+R", false, view_model_ && view_model_->isGameActive())) {
                show_reset_dialog_ = true;
            }

            ImGui::Separator();

            if (ImGui::MenuItem(loc(MenuSave), "Ctrl+S", false, view_model_ && view_model_->isGameActive())) {
                show_save_dialog_ = true;
            }

            if (ImGui::MenuItem(loc(MenuLoad), "Ctrl+O")) {
                show_load_dialog_ = true;
            }

            ImGui::Separator();

            if (app_mode_ == AppMode::Play) {
                if (ImGui::MenuItem("Training Mode", nullptr, false, training_vm_ != nullptr)) {
                    app_mode_ = AppMode::Training;
                }
            } else {
                if (ImGui::MenuItem("Resume Game")) {
                    app_mode_ = AppMode::Play;
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem(loc(MenuStatistics))) {
                show_statistics_ = true;
            }

            if (ImGui::MenuItem(loc(MenuExportAggregate))) {
                exportAggregateStatsCsv();
            }

            if (ImGui::MenuItem(loc(MenuExportSessions))) {
                exportGameSessionsCsv();
            }

            ImGui::Separator();

            if (ImGui::MenuItem(loc(MenuExit), "Alt+F4")) {
                should_close_ = true;
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(loc(MenuEdit))) {
            bool can_undo = view_model_ && view_model_->canUndo();
            bool can_redo = view_model_ && view_model_->canRedo();

            if (ImGui::MenuItem(loc(MenuUndo), "Ctrl+Z", false, can_undo)) {
                if (view_model_) {
                    view_model_->undo();
                }
            }

            if (ImGui::MenuItem(loc(MenuRedo), "Ctrl+Y", false, can_redo)) {
                if (view_model_) {
                    view_model_->redo();
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem(loc(MenuClearCell), "Delete", false, view_model_ && view_model_->isGameActive())) {
                if (view_model_) {
                    view_model_->clearSelectedCell();
                }
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(loc(MenuHelp))) {
            if (ImGui::MenuItem(loc(MenuGetHint), "H", false, view_model_ && view_model_->getHintCount() > 0)) {
                if (view_model_) {
                    view_model_->getHint();
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem(loc(MenuAbout))) {
                show_about_ = true;
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void MainWindow::renderToolbar() {
    ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetFrameHeight()), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 50), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 12));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.961f, 0.961f, 0.961f, 1.0f));  // #f5f5f5
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.867f, 0.867f, 0.867f, 1.0f));    // #ddd
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    ImGui::Begin("Toolbar", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar);

    // New Game button with blue background
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.145f, 0.388f, 0.922f, 1.0f));         // #2563eb
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.114f, 0.306f, 0.847f, 1.0f));  // #1d4ed8
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.098f, 0.259f, 0.722f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));  // White text

    if (ImGui::Button(loc(ToolbarNewGame), ImVec2(120, 30))) {
        show_new_game_dialog_ = true;
    }

    ImGui::PopStyleColor(4);

    ImGui::SameLine(0, 30);

    ImGui::Text("%s", loc(ToolbarDifficulty));
    ImGui::SameLine();

    const std::array<const char*, 5> difficulties = {loc(DifficultyEasy), loc(DifficultyMedium), loc(DifficultyHard),
                                                     loc(DifficultyExpert), loc(DifficultyMaster)};
    ImGui::SetNextItemWidth(120);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
    ImGui::Combo("##Difficulty", &selected_difficulty_, difficulties.data(), static_cast<int>(difficulties.size()));
    ImGui::PopStyleVar();

    ImGui::SameLine(0, 30);

    ImGui::Text("%s", loc(ToolbarHints));
    ImGui::SameLine();

    // Hints counter with badge style
    int hint_count = view_model_ ? view_model_->getHintCount() : 10;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.145f, 0.388f, 0.922f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.145f, 0.388f, 0.922f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.145f, 0.388f, 0.922f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 2));

    std::string hint_text = std::to_string(hint_count);
    ImGui::Button(hint_text.c_str(), ImVec2(0, 24));

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    // Display puzzle rating if available
    if (view_model_) {
        const auto& ui_state = view_model_->uiState.get();
        if (ui_state.puzzle_rating > 0) {
            ImGui::SameLine(0, 30);
            ImGui::Text("%s", loc(ToolbarRating));
            ImGui::SameLine(0, 5);  // Small spacing
            ImGui::Text("%d", ui_state.puzzle_rating);
            ImGui::SameLine(0, 8);  // Slightly more spacing before help icon
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));  // Gray color
            ImGui::Text("%s", loc(ToolbarHelpIcon));
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) {
                renderRatingTooltip(ui_state);
            }
        }
    }

    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void MainWindow::renderStatusBar() {
    float status_bar_height = 30.0f;
    ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - status_bar_height), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, status_bar_height), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 6));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.941f, 0.941f, 0.941f, 1.0f));  // #f0f0f0
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.867f, 0.867f, 0.867f, 1.0f));    // #ddd
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));  // #666

    ImGui::Begin("Status", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar);

    // Left side - status message
    if (view_model_) {
        const auto& game_state = view_model_->gameState.get();
        if (game_state.isComplete()) {
            ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), "%s", loc(StatusCompleted));
        } else if (game_state.isTimerRunning()) {
            ImGui::Text("%s", loc(StatusPlaying));
        } else {
            ImGui::Text("%s", loc(StatusReady));
        }
    } else {
        ImGui::Text("%s", loc(StatusReady));
    }

    // Right side - game state or menu hint
    if (!menu_visible_) {
        ImGui::SameLine();
        float hint_width = ImGui::CalcTextSize(loc(StatusPressF1)).x;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - hint_width - 20.0f);
        ImGui::TextDisabled("%s", loc(StatusPressF1));
    } else {
        float right_text_width = ImGui::CalcTextSize(loc(StatusPlaying)).x + 16;
        ImGui::SameLine(ImGui::GetWindowWidth() - right_text_width);

        if (view_model_ && view_model_->gameState.get().isTimerRunning()) {
            ImGui::Text("%s", loc(StatusPlaying));
        } else {
            ImGui::Text("%s", loc(StatusReady));
        }
    }

    ImGui::End();

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
}

void MainWindow::renderSidebar() {
    ImGui::Begin("Info");

    ImGui::Text("%s", loc(AboutSudokuGame));
    ImGui::Separator();

    if (view_model_) {
        const auto& game_state = view_model_->gameState.get();
        const auto& ui_state = view_model_->uiState.get();

        auto diff_text = locFormat(SidebarDifficulty, difficultyString(game_state.getDifficulty()));
        ImGui::Text("%s", diff_text.c_str());

        // Display puzzle rating if available
        if (ui_state.puzzle_rating > 0) {
            auto rating_text = locFormat(SidebarRating, ui_state.puzzle_rating);
            ImGui::Text("%s", rating_text.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                renderRatingTooltip(ui_state);
            }
        }

        if (game_state.isComplete()) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", loc(StatusCompleted));
        } else if (game_state.isTimerRunning()) {
            ImGui::TextColored(ImVec4(0, 0.5f, 1, 1), "%s", loc(StatusInProgress));
        }
    }

    // Language selector
    if (loc_manager_) {
        ImGui::Separator();
        ImGui::Text("%s", loc(SidebarLanguage));

        auto available = loc_manager_->getAvailableLocales();
        std::vector<const char*> locale_names;
        locale_names.reserve(available.size());
        for (const auto& [code, name] : available) {
            locale_names.push_back(name.c_str());
        }

        // Sync selection index with current locale
        auto current = loc_manager_->getCurrentLocale();
        for (int i = 0; std::cmp_less(i, available.size()); ++i) {
            if (available[i].first == current) {
                selected_language_ = i;
                break;
            }
        }

        ImGui::SetNextItemWidth(-1);  // Fill available width
        if (ImGui::Combo("##Language", &selected_language_, locale_names.data(),
                         static_cast<int>(locale_names.size()))) {
            auto result = loc_manager_->setLocale(available[selected_language_].first);
            if (result) {
                saveLanguagePreference(available[selected_language_].first);
            } else {
                spdlog::warn("Failed to switch locale: {}", result.error());
            }
        }
    }

    ImGui::End();
}

void MainWindow::exportAggregateStatsCsv() {
    if (!view_model_) {
        return;
    }

    auto result = view_model_->exportAggregateStatsCsv();
    if (result) {
        toast_.show(loc(ToastAggregateExported));
        spdlog::info("Aggregate stats exported successfully via UI");
    } else {
        toast_.show(locFormat(ToastExportFailed, result.error()));
        spdlog::error("Failed to export aggregate stats via UI: {}", result.error());
    }
}

void MainWindow::exportGameSessionsCsv() {
    if (!view_model_) {
        return;
    }

    auto result = view_model_->exportGameSessionsCsv();
    if (result) {
        toast_.show(loc(ToastSessionsExported));
        spdlog::info("Game sessions exported successfully via UI");
    } else {
        toast_.show(locFormat(ToastExportFailed, result.error()));
        spdlog::error("Failed to export game sessions via UI: {}", result.error());
    }
}

void MainWindow::renderRatingTooltip(const viewmodel::UIState& ui_state) {
    using core::Difficulty;
    using core::getDifficultyRatingRange;

    constexpr std::array<std::pair<Difficulty, std::string_view>, core::DIFFICULTY_COUNT> difficulty_keys = {{
        {Difficulty::Easy, DifficultyEasy},
        {Difficulty::Medium, DifficultyMedium},
        {Difficulty::Hard, DifficultyHard},
        {Difficulty::Expert, DifficultyExpert},
        {Difficulty::Master, DifficultyMaster},
    }};

    ImGui::BeginTooltip();
    ImGui::Text("%s", loc(TooltipRatingScale));
    ImGui::Separator();
    for (const auto& [difficulty, key] : difficulty_keys) {
        auto [min_rating, max_rating] = getDifficultyRatingRange(difficulty);
        if (max_rating == std::numeric_limits<int>::max()) {
            ImGui::BulletText("%d+: %s", min_rating, loc(key));
        } else {
            ImGui::BulletText("%d-%d: %s", min_rating, max_rating, loc(key));
        }
    }

    if (!ui_state.puzzle_techniques.empty()) {
        ImGui::Spacing();
        ImGui::Text("%s", loc(TooltipTechniquesRequired));
        ImGui::Separator();
        for (const auto& technique : ui_state.puzzle_techniques) {
            ImGui::BulletText("%s", technique.c_str());
        }
    }
    ImGui::EndTooltip();
}

}  // namespace sudoku::view
