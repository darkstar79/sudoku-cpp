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

#include "main_window.h"

#include <array>

#include <fmt/format.h>
#include <imgui.h>

namespace sudoku::view {

void MainWindow::renderTrainingMode() {
    if (!training_vm_) {
        return;
    }

    const auto& state = training_vm_->trainingState.get();

    switch (state.phase) {
        case core::TrainingPhase::TechniqueSelection:
            renderTechniqueSelection();
            break;
        case core::TrainingPhase::TheoryReview:
            renderTheoryReview();
            break;
        case core::TrainingPhase::Exercise:
            renderTrainingExercise();
            break;
        case core::TrainingPhase::Feedback:
            renderTrainingFeedback();
            break;
        case core::TrainingPhase::LessonComplete:
            renderLessonComplete();
            break;
    }
}

void MainWindow::renderTechniqueSelection() {
    ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetFrameHeight()), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);

    ImGui::Begin("Training - Select Technique", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    ImGui::PushFont(font_manager_->getFont(FontType::Large));
    ImGui::Text("Training Mode");
    ImGui::PopFont();
    ImGui::Text("Select a technique to practice:");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Technique groups with collapsible headers
    struct TechniqueGroup {
        const char* name;
        std::vector<core::SolvingTechnique> techniques;
    };

    // clang-format off
    std::array<TechniqueGroup, 9> groups = {{
        {.name = "Foundations", .techniques = {core::SolvingTechnique::NakedSingle, core::SolvingTechnique::HiddenSingle}},
        {.name = "Subset Basics", .techniques = {core::SolvingTechnique::NakedPair, core::SolvingTechnique::NakedTriple,
                           core::SolvingTechnique::HiddenPair, core::SolvingTechnique::HiddenTriple}},
        {.name = "Intersections & Quads", .techniques = {core::SolvingTechnique::PointingPair, core::SolvingTechnique::BoxLineReduction,
                                    core::SolvingTechnique::NakedQuad, core::SolvingTechnique::HiddenQuad}},
        {.name = "Basic Fish & Wings", .techniques = {core::SolvingTechnique::XWing, core::SolvingTechnique::XYWing,
                                 core::SolvingTechnique::Swordfish, core::SolvingTechnique::Skyscraper,
                                 core::SolvingTechnique::TwoStringKite, core::SolvingTechnique::XYZWing}},
        {.name = "Links & Rectangles", .techniques = {core::SolvingTechnique::UniqueRectangle, core::SolvingTechnique::WWing,
                                 core::SolvingTechnique::SimpleColoring, core::SolvingTechnique::FinnedXWing,
                                 core::SolvingTechnique::RemotePairs, core::SolvingTechnique::BUG}},
        {.name = "Advanced Fish & Wings", .techniques = {core::SolvingTechnique::Jellyfish, core::SolvingTechnique::FinnedSwordfish,
                                    core::SolvingTechnique::EmptyRectangle, core::SolvingTechnique::WXYZWing,
                                    core::SolvingTechnique::MultiColoring}},
        {.name = "Advanced Fish (Finned)", .techniques = {core::SolvingTechnique::FinnedJellyfish}},
        {.name = "Chains & Set Logic", .techniques = {core::SolvingTechnique::XYChain, core::SolvingTechnique::ALSxZ,
                                 core::SolvingTechnique::SueDeCoq}},
        {.name = "Inference Engines", .techniques = {core::SolvingTechnique::ForcingChain, core::SolvingTechnique::NiceLoop}},
    }};
    // clang-format on

    for (const auto& group : groups) {
        if (ImGui::CollapsingHeader(group.name, ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto technique : group.techniques) {
                auto name = core::getTechniqueName(technique);
                auto points = core::getTechniquePoints(technique);
                auto label = fmt::format("{} ({} pts)", name, points);

                if (ImGui::Selectable(label.c_str())) {
                    training_vm_->selectTechnique(technique);
                }
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Back to Game", ImVec2(150, 35))) {
        app_mode_ = AppMode::Play;
    }

    ImGui::End();
}

void MainWindow::renderTheoryReview() {
    ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetFrameHeight()), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);

    ImGui::Begin("Training - Theory", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    auto desc = training_vm_->currentDescription();
    const auto& state = training_vm_->trainingState.get();
    auto points = core::getTechniquePoints(state.current_technique);

    ImGui::PushFont(font_manager_->getFont(FontType::Large));
    ImGui::Text("%.*s", static_cast<int>(desc.title.size()), desc.title.data());
    ImGui::PopFont();

    ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "%d difficulty points", points);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("What It Is:");
    ImGui::Spacing();
    ImGui::TextWrapped("%.*s", static_cast<int>(desc.what_it_is.size()), desc.what_it_is.data());
    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::Text("What to Look For:");
    ImGui::Spacing();
    ImGui::TextWrapped("%.*s", static_cast<int>(desc.what_to_look_for.size()), desc.what_to_look_for.data());

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Start Exercises", ImVec2(160, 40))) {
        training_vm_->startExercises();
    }

    ImGui::SameLine(0, 20);

    if (ImGui::Button("Back", ImVec2(100, 40))) {
        training_vm_->returnToSelection();
    }

    ImGui::End();
}

void MainWindow::renderTrainingExercise() {
    const auto& state = training_vm_->trainingState.get();
    const auto& board = training_vm_->trainingBoard.get();

    ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetFrameHeight()), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);

    ImGui::Begin("Training - Exercise", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    // Header
    auto technique_name = core::getTechniqueName(state.current_technique);
    ImGui::Text("Exercise %d / %d  -  %.*s", state.exercise_index + 1, state.total_exercises,
                static_cast<int>(technique_name.size()), technique_name.data());

    if (state.current_hint_level > 0) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "(Hint level %d/3)", state.current_hint_level);
    }

    ImGui::Spacing();

    // Render the training board
    float available = ImGui::GetContentRegionAvail().x;
    float board_size = std::min(available, UIConstants::BOARD_MAX_SIZE);
    board_size = std::max(board_size, UIConstants::BOARD_MIN_SIZE);
    renderTrainingBoard(board, board_size);

    ImGui::Spacing();

    // Action buttons
    if (ImGui::Button("Submit", ImVec2(100, 35))) {
        training_vm_->submitAnswer(board);
    }

    ImGui::SameLine(0, 15);

    if (ImGui::Button("Hint", ImVec2(80, 35))) {
        training_vm_->requestHint();
    }

    ImGui::SameLine(0, 15);

    if (ImGui::Button("Skip", ImVec2(80, 35))) {
        training_vm_->skipExercise();
    }

    ImGui::SameLine(0, 30);

    if (ImGui::Button("Quit Lesson", ImVec2(120, 35))) {
        training_vm_->returnToSelection();
    }

    ImGui::End();
}

void MainWindow::renderTrainingFeedback() {
    const auto& state = training_vm_->trainingState.get();

    ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetFrameHeight()), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);

    ImGui::Begin("Training - Feedback", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    // Result header
    ImVec4 result_color{};
    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores) — safe default if enum gains new values
    const char* result_label = "";
    switch (state.last_result) {
        case core::AnswerResult::Correct:
            result_color = ImVec4(0.0f, 0.6f, 0.0f, 1.0f);
            result_label = "Correct!";
            break;
        case core::AnswerResult::PartiallyCorrect:
            result_color = ImVec4(0.8f, 0.6f, 0.0f, 1.0f);
            result_label = "Partially Correct";
            break;
        case core::AnswerResult::Incorrect:
            result_color = ImVec4(0.8f, 0.0f, 0.0f, 1.0f);
            result_label = "Incorrect";
            break;
    }

    ImGui::PushFont(font_manager_->getFont(FontType::Large));
    ImGui::TextColored(result_color, "%s", result_label);
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::TextWrapped("%s", state.feedback_message.c_str());

    ImGui::Spacing();
    ImGui::Text("Score: %d / %d", state.correct_count, state.exercise_index + 1);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Next Exercise", ImVec2(140, 40))) {
        training_vm_->nextExercise();
    }

    ImGui::SameLine(0, 15);

    if (ImGui::Button("Retry", ImVec2(100, 40))) {
        training_vm_->retryExercise();
    }

    ImGui::SameLine(0, 30);

    if (ImGui::Button("Quit Lesson", ImVec2(120, 40))) {
        training_vm_->returnToSelection();
    }

    ImGui::End();
}

void MainWindow::renderLessonComplete() {
    const auto& state = training_vm_->trainingState.get();

    ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetFrameHeight()), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);

    ImGui::Begin("Training - Complete", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    auto technique_name = core::getTechniqueName(state.current_technique);

    ImGui::PushFont(font_manager_->getFont(FontType::Large));
    ImGui::Text("Lesson Complete!");
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::Text("Technique: %.*s", static_cast<int>(technique_name.size()), technique_name.data());
    ImGui::Text("Score: %d / %d correct", state.correct_count, state.total_exercises);
    ImGui::Text("Hints used: %d", state.hints_used);

    ImGui::Spacing();

    float ratio = state.total_exercises > 0
                      ? static_cast<float>(state.correct_count) / static_cast<float>(state.total_exercises)
                      : 0.0f;
    if (ratio >= 0.8f) {
        ImGui::TextColored(ImVec4(0.0f, 0.6f, 0.0f, 1.0f), "Excellent! You've mastered this technique.");
    } else if (ratio >= 0.5f) {
        ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.0f, 1.0f), "Good progress. Try again for a higher score.");
    } else {
        ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1.0f), "Keep practicing! Review the theory and try again.");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Try Again", ImVec2(120, 40))) {
        training_vm_->selectTechnique(state.current_technique);
    }

    ImGui::SameLine(0, 15);

    if (ImGui::Button("Pick Technique", ImVec2(140, 40))) {
        training_vm_->returnToSelection();
    }

    ImGui::SameLine(0, 15);

    if (ImGui::Button("Return to Game", ImVec2(140, 40))) {
        training_vm_->returnToSelection();
        app_mode_ = AppMode::Play;
    }

    ImGui::End();
}

void MainWindow::renderTrainingBoard(const core::TrainingBoard& board, float board_size) {
    float cell_size = board_size / 9.0f;
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Board background
    draw_list->AddRectFilled(cursor, ImVec2(cursor.x + board_size, cursor.y + board_size),
                             UIConstants::Colors::BOARD_BACKGROUND);
    draw_list->AddRect(cursor, ImVec2(cursor.x + board_size, cursor.y + board_size), UIConstants::Colors::BOARD_BORDER,
                       0.0f, 0, THICK_LINE_WIDTH);

    // Render cells
    for (size_t row = 0; row < 9; ++row) {
        for (size_t col = 0; col < 9; ++col) {
            const auto& cell = board[row][col];
            ImVec2 cell_pos(cursor.x + (static_cast<float>(col) * cell_size),
                            cursor.y + (static_cast<float>(row) * cell_size));

            // Cell background based on role
            unsigned int bg_color = UIConstants::Colors::CELL_BACKGROUND;
            if (cell.player_selected || cell.highlight_role != core::CellRole::Pattern) {
                bg_color = getCellRoleColor(cell.highlight_role);
            }
            draw_list->AddRectFilled(cell_pos, ImVec2(cell_pos.x + cell_size, cell_pos.y + cell_size), bg_color);

            if (cell.value != 0) {
                // Render placed value
                auto text = fmt::format("{}", cell.value);
                unsigned int text_color =
                    cell.is_given ? UIConstants::Colors::TEXT_GIVEN : UIConstants::Colors::TEXT_USER;
                ImVec2 text_size = ImGui::CalcTextSize(text.c_str());
                ImVec2 text_pos(cell_pos.x + ((cell_size - text_size.x) * 0.5f),
                                cell_pos.y + ((cell_size - text_size.y) * 0.5f));
                draw_list->AddText(text_pos, text_color, text.c_str());
            } else if (!cell.candidates.empty()) {
                // Render candidates in 3x3 mini-grid
                float note_size = cell_size / 3.0f;
                for (int v : cell.candidates) {
                    int ni = (v - 1) / 3;  // row in mini-grid
                    int nj = (v - 1) % 3;  // col in mini-grid
                    auto note_text = fmt::format("{}", v);
                    ImVec2 note_text_size = ImGui::CalcTextSize(note_text.c_str());
                    ImVec2 note_pos(
                        cell_pos.x + (static_cast<float>(nj) * note_size) + ((note_size - note_text_size.x) * 0.5f),
                        cell_pos.y + (static_cast<float>(ni) * note_size) + ((note_size - note_text_size.y) * 0.5f));
                    draw_list->AddText(note_pos, UIConstants::Colors::TEXT_NOTE, note_text.c_str());
                }
            }
        }
    }

    // Grid lines
    for (int i = 0; i <= 9; ++i) {
        float pos = static_cast<float>(i) * cell_size;
        float thickness = (i % 3 == 0) ? THICK_LINE_WIDTH : THIN_LINE_WIDTH;
        unsigned int color = (i % 3 == 0) ? UIConstants::Colors::GRID_THICK_LINE : UIConstants::Colors::GRID_THIN_LINE;

        // Horizontal
        draw_list->AddLine(ImVec2(cursor.x, cursor.y + pos), ImVec2(cursor.x + board_size, cursor.y + pos), color,
                           thickness);
        // Vertical
        draw_list->AddLine(ImVec2(cursor.x + pos, cursor.y), ImVec2(cursor.x + pos, cursor.y + board_size), color,
                           thickness);
    }

    // Reserve space for the board in ImGui layout
    ImGui::Dummy(ImVec2(board_size, board_size));
}

unsigned int MainWindow::getCellRoleColor(core::CellRole role) {
    switch (role) {
        case core::CellRole::Pattern:
        case core::CellRole::Pivot:
        case core::CellRole::Wing:
        case core::CellRole::Fin:
            return UIConstants::Colors::TRAIN_PATTERN;
        case core::CellRole::Roof:
        case core::CellRole::Floor:
            return UIConstants::Colors::TRAIN_REGION;
        case core::CellRole::ChainA:
            return UIConstants::Colors::TRAIN_CHAIN_A;
        case core::CellRole::ChainB:
            return UIConstants::Colors::TRAIN_CHAIN_B;
        case core::CellRole::LinkEndpoint:
            return UIConstants::Colors::TRAIN_REGION;
        case core::CellRole::SetA:
            return UIConstants::Colors::TRAIN_CHAIN_A;
        case core::CellRole::SetB:
        case core::CellRole::SetC:
            return UIConstants::Colors::TRAIN_CHAIN_B;
    }
    return UIConstants::Colors::CELL_BACKGROUND;
}

}  // namespace sudoku::view
