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
#include "main_window.h"

#include <cstring>

#include <imgui.h>

namespace sudoku::view {

using namespace core::StringKeys;

void MainWindow::renderNewGameDialog() {
    ImGui::SetNextWindowSize(ImVec2(250, 230), ImGuiCond_Once);
    if (ImGui::Begin(loc(DialogNewGame), &show_new_game_dialog_)) {
        ImGui::Text("%s", loc(DialogSelectDifficulty));

        ImGui::RadioButton(loc(DifficultyEasy), &selected_difficulty_, 0);
        ImGui::RadioButton(loc(DifficultyMedium), &selected_difficulty_, 1);
        ImGui::RadioButton(loc(DifficultyHard), &selected_difficulty_, 2);
        ImGui::RadioButton(loc(DifficultyExpert), &selected_difficulty_, 3);
        ImGui::RadioButton(loc(DifficultyMaster), &selected_difficulty_, 4);

        ImGui::Separator();

        if (ImGui::Button(loc(DialogStartGame))) {
            if (view_model_) {
                auto difficulty = static_cast<core::Difficulty>(selected_difficulty_);
                view_model_->startNewGame(difficulty);
            }
            show_new_game_dialog_ = false;
        }

        ImGui::SameLine();

        if (ImGui::Button(loc(DialogCancel))) {
            show_new_game_dialog_ = false;
        }
    }
    ImGui::End();
}

void MainWindow::renderResetDialog() {
    if (ImGui::Begin(loc(DialogResetPuzzle), &show_reset_dialog_)) {
        ImGui::TextWrapped("%s", loc(DialogResetWarning));

        ImGui::Separator();

        // Red-styled Reset button to indicate destructive action
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.863F, 0.149F, 0.149F, 1.0F));         // #DC2626
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.725F, 0.110F, 0.110F, 1.0F));  // darker red
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.600F, 0.090F, 0.090F, 1.0F));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0F, 1.0F, 1.0F, 1.0F));

        if (ImGui::Button(loc(DialogReset))) {
            if (view_model_) {
                view_model_->executeCommand(viewmodel::GameCommand::ResetGame);
            }
            show_reset_dialog_ = false;
        }

        ImGui::PopStyleColor(4);

        ImGui::SameLine();

        if (ImGui::Button(loc(DialogCancel))) {
            show_reset_dialog_ = false;
        }
    }
    ImGui::End();
}

void MainWindow::renderSaveDialog() {
    if (ImGui::Begin(loc(DialogSaveGame), &show_save_dialog_)) {
        ImGui::Text("%s", loc(DialogEnterSaveName));

        // Ensure buffer has enough capacity for ImGui
        save_name_buffer_.resize(UIConstants::SAVE_NAME_BUFFER_SIZE);
        if (ImGui::InputText("##SaveName", save_name_buffer_.data(), save_name_buffer_.capacity())) {
            // Resize to actual string length (remove null padding)
            save_name_buffer_.resize(std::strlen(save_name_buffer_.c_str()));
        }

        ImGui::Separator();

        if (ImGui::Button(loc(DialogSave))) {
            if (view_model_ && !save_name_buffer_.empty()) {
                bool success = view_model_->saveCurrentGame(save_name_buffer_);
                if (success) {
                    toast_.show(loc(ToastGameSaved));
                }
            }
            show_save_dialog_ = false;
            save_name_buffer_.clear();
        }

        ImGui::SameLine();

        if (ImGui::Button(loc(DialogCancel))) {
            show_save_dialog_ = false;
            save_name_buffer_.clear();
        }
    }
    ImGui::End();
}

void MainWindow::renderLoadDialog() {
    if (ImGui::Begin(loc(DialogLoadGame), &show_load_dialog_)) {
        ImGui::Text("%s", loc(DialogRecentSaves));

        if (view_model_) {
            auto saves = view_model_->recentSaves.get();
            for (const auto& save : saves) {
                if (ImGui::Selectable(save.c_str())) {
                    view_model_->loadGame(save);
                    show_load_dialog_ = false;
                    break;
                }
            }
        }

        ImGui::Separator();

        if (ImGui::Button(loc(DialogCancel))) {
            show_load_dialog_ = false;
        }
    }
    ImGui::End();
}

void MainWindow::renderStatisticsDialog() {
    if (ImGui::Begin(loc(DialogStatistics), &show_statistics_)) {
        if (view_model_) {
            auto stats = view_model_->statistics.get();

            ImGui::Text("%s", locFormat(StatsGamesPlayed, stats.games_played).c_str());
            ImGui::Text("%s", locFormat(StatsGamesCompleted, stats.games_completed).c_str());
            ImGui::Text("%s", locFormat(StatsCompletionRate, stats.completion_rate).c_str());
            ImGui::Text("%s", locFormat(StatsBestTime, stats.best_time).c_str());
            ImGui::Text("%s", locFormat(StatsAverageTime, stats.average_time).c_str());
            ImGui::Text("%s", locFormat(StatsCurrentStreak, stats.current_streak).c_str());
            ImGui::Text("%s", locFormat(StatsBestStreak, stats.best_streak).c_str());
        }

        ImGui::Separator();

        if (ImGui::Button(loc(DialogClose))) {
            show_statistics_ = false;
        }
    }
    ImGui::End();
}

void MainWindow::renderAboutDialog() {
    if (ImGui::Begin(loc(DialogAbout), &show_about_)) {
        ImGui::Text("%s", loc(AboutSudokuGame));
        ImGui::Text("%s", loc(AboutPhaseInfo));
        ImGui::Separator();
        ImGui::Text("%s", loc(AboutBuiltWith));
        ImGui::BulletText("SDL3");
        ImGui::BulletText("Dear ImGui");
        ImGui::BulletText("C++23");

        ImGui::Separator();

        if (ImGui::Button(loc(DialogClose))) {
            show_about_ = false;
        }
    }
    ImGui::End();
}

}  // namespace sudoku::view
