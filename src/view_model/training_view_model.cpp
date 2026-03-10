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

#include "training_view_model.h"

#include "core/i_game_validator.h"
#include "core/i_localization_manager.h"
#include "core/i_training_exercise_generator.h"
#include "core/observable.h"
#include "core/technique_descriptions.h"
#include "core/training_types.h"

#include <algorithm>
#include <array>
#include <expected>
#include <set>
#include <tuple>
#include <utility>

#include <stddef.h>
#include <stdint.h>

#include <fmt/base.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace sudoku::viewmodel {

using namespace core;

TrainingViewModel::TrainingViewModel(std::shared_ptr<ITrainingExerciseGenerator> exercise_generator,
                                     std::shared_ptr<ILocalizationManager> loc_manager)
    : exercise_generator_(std::move(exercise_generator)), loc_manager_(std::move(loc_manager)) {
}

void TrainingViewModel::selectTechnique(SolvingTechnique technique) {
    if (technique == SolvingTechnique::Backtracking) {
        errorMessage.set("Cannot practice Backtracking — it is not a logical technique.");
        return;
    }

    trainingState.update([technique](TrainingUIState& state) {
        state.phase = TrainingPhase::TheoryReview;
        state.current_technique = technique;
        state.exercise_index = 0;
        state.correct_count = 0;
        state.hints_used = 0;
        state.current_hint_level = 0;
        state.feedback_message.clear();
    });
}

void TrainingViewModel::startExercises() {
    const auto& state = trainingState.get();
    if (state.phase != TrainingPhase::TheoryReview) {
        return;
    }

    auto result = exercise_generator_->generateExercises(state.current_technique, state.total_exercises);

    if (!result.has_value()) {
        errorMessage.set(result.error());
        spdlog::warn("Failed to generate exercises: {}", result.error());
        return;
    }

    exercises_ = std::move(*result);

    trainingState.update([this](TrainingUIState& s) {
        s.phase = TrainingPhase::Exercise;
        s.exercise_index = 0;
        s.total_exercises = static_cast<int>(exercises_.size());
        s.current_hint_level = 0;
    });

    loadExerciseBoard();
}

void TrainingViewModel::submitAnswer(const TrainingBoard& player_board) {
    const auto& state = trainingState.get();
    if (state.phase != TrainingPhase::Exercise) {
        return;
    }

    auto idx = static_cast<size_t>(state.exercise_index);
    if (idx >= exercises_.size()) {
        return;
    }

    const auto& exercise = exercises_[idx];
    const auto& expected = exercise.expected_step;

    AnswerResult result{};
    if (exercise.interaction_mode == TrainingInteractionMode::Placement) {
        result = evaluatePlacement(player_board, expected);
    } else {
        result = evaluateElimination(player_board, expected);
    }

    auto feedback = buildFeedback(result, expected);

    trainingState.update([result, &feedback](TrainingUIState& s) {
        s.phase = TrainingPhase::Feedback;
        s.last_result = result;
        s.feedback_message = feedback;
        if (result == AnswerResult::Correct) {
            s.correct_count++;
        }
    });
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — hint flow with step type + phase branching; nesting is inherent
void TrainingViewModel::requestHint() {
    const auto& state = trainingState.get();
    if (state.phase != TrainingPhase::Exercise || state.current_hint_level >= 3) {
        return;
    }

    auto idx = static_cast<size_t>(state.exercise_index);
    if (idx >= exercises_.size()) {
        return;
    }

    const auto& exercise = exercises_[idx];
    const auto& expected = exercise.expected_step;

    int new_level = state.current_hint_level + 1;

    trainingState.update([new_level](TrainingUIState& s) {
        s.current_hint_level = new_level;
        s.hints_used++;
    });

    // Update board with progressive hints
    trainingBoard.update([&expected, new_level](TrainingBoard& board) {
        if (new_level >= 2) {
            // Level 2+: Highlight pattern cells with role-specific colors
            const auto& roles = expected.explanation_data.position_roles;
            for (size_t i = 0; i < expected.explanation_data.positions.size(); ++i) {
                const auto& pos = expected.explanation_data.positions[i];
                if (pos.row < 9 && pos.col < 9) {
                    board[pos.row][pos.col].highlight_role = (i < roles.size()) ? roles[i] : CellRole::Pattern;
                    board[pos.row][pos.col].player_selected = true;
                }
            }
        }

        if (new_level >= 3) {
            // Level 3: Full reveal — show eliminations
            if (expected.type == SolveStepType::Elimination) {
                for (const auto& elim : expected.eliminations) {
                    if (elim.position.row < 9 && elim.position.col < 9) {
                        board[elim.position.row][elim.position.col].player_selected = true;
                    }
                }
            }
        }
    });
}

void TrainingViewModel::skipExercise() {
    const auto& state = trainingState.get();
    if (state.phase != TrainingPhase::Exercise) {
        return;
    }

    int next_idx = state.exercise_index + 1;
    if (next_idx >= state.total_exercises) {
        trainingState.update([](TrainingUIState& s) { s.phase = TrainingPhase::LessonComplete; });
    } else {
        trainingState.update([next_idx](TrainingUIState& s) {
            s.exercise_index = next_idx;
            s.current_hint_level = 0;
        });
        loadExerciseBoard();
    }
}

void TrainingViewModel::retryExercise() {
    const auto& state = trainingState.get();
    if (state.phase != TrainingPhase::Feedback && state.phase != TrainingPhase::Exercise) {
        return;
    }

    trainingState.update([](TrainingUIState& s) {
        s.phase = TrainingPhase::Exercise;
        s.current_hint_level = 0;
    });

    loadExerciseBoard();
}

void TrainingViewModel::nextExercise() {
    const auto& state = trainingState.get();
    if (state.phase != TrainingPhase::Feedback) {
        return;
    }

    int next_idx = state.exercise_index + 1;
    if (next_idx >= state.total_exercises) {
        trainingState.update([](TrainingUIState& s) { s.phase = TrainingPhase::LessonComplete; });
    } else {
        trainingState.update([next_idx](TrainingUIState& s) {
            s.phase = TrainingPhase::Exercise;
            s.exercise_index = next_idx;
            s.current_hint_level = 0;
        });
        loadExerciseBoard();
    }
}

void TrainingViewModel::returnToSelection() {
    exercises_.clear();
    trainingState.set(TrainingUIState{});  // Reset to defaults (TechniqueSelection)
    trainingBoard.set(TrainingBoard{});
}

TechniqueDescription TrainingViewModel::currentDescription() const {
    return getTechniqueDescription(trainingState.get().current_technique);
}

// --- Private helpers ---

void TrainingViewModel::loadExerciseBoard() {
    auto idx = static_cast<size_t>(trainingState.get().exercise_index);
    if (idx >= exercises_.size()) {
        return;
    }

    const auto& exercise = exercises_[idx];
    TrainingBoard board{};

    for (size_t r = 0; r < 9; ++r) {
        for (size_t c = 0; c < 9; ++c) {
            auto& cell = board[r][c];
            int value = exercise.board[r][c];
            cell.value = value;
            cell.is_given = (value != 0);

            if (value == 0) {
                // Populate candidates from bitmask
                uint16_t mask = exercise.candidate_masks[(r * 9) + c];
                for (int v = 1; v <= 9; ++v) {
                    if ((mask & (1 << v)) != 0) {
                        cell.candidates.push_back(v);
                    }
                }
            }
        }
    }

    trainingBoard.set(board);
}

AnswerResult TrainingViewModel::evaluatePlacement(const TrainingBoard& player_board, const SolveStep& expected) {
    const auto& pos = expected.position;
    if (pos.row >= 9 || pos.col >= 9) {
        return AnswerResult::Incorrect;
    }

    const auto& cell = player_board[pos.row][pos.col];
    if (cell.value == expected.value) {
        return AnswerResult::Correct;
    }

    // Check if the player placed the right value somewhere else
    for (size_t r = 0; r < 9; ++r) {
        for (size_t c = 0; c < 9; ++c) {
            if (player_board[r][c].player_selected && player_board[r][c].value == expected.value) {
                return AnswerResult::Incorrect;  // Right value, wrong cell
            }
        }
    }

    return AnswerResult::Incorrect;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — evaluates player's elimination against expected step; set-difference logic with multiple correctness criteria; nesting is inherent
AnswerResult TrainingViewModel::evaluateElimination(const TrainingBoard& player_board,
                                                    const SolveStep& expected) const {
    // Build set of expected eliminations: {(row, col, value)}
    std::set<std::tuple<size_t, size_t, int>> expected_set;
    for (const auto& elim : expected.eliminations) {
        expected_set.emplace(elim.position.row, elim.position.col, elim.value);
    }

    // Build set of player-marked eliminations
    // A player "eliminates" by selecting cells and marking candidates
    std::set<std::tuple<size_t, size_t, int>> player_set;
    for (size_t r = 0; r < 9; ++r) {
        for (size_t c = 0; c < 9; ++c) {
            if (player_board[r][c].player_selected) {
                // Find which candidates the player removed vs the exercise board
                const auto& exercise_idx = static_cast<size_t>(trainingState.get().exercise_index);
                if (exercise_idx < exercises_.size()) {
                    uint16_t original_mask = exercises_[exercise_idx].candidate_masks[(r * 9) + c];
                    for (int v = 1; v <= 9; ++v) {
                        bool was_candidate = (original_mask & (1 << v)) != 0;
                        bool still_in_player = std::ranges::contains(player_board[r][c].candidates, v);
                        if (was_candidate && !still_in_player) {
                            player_set.emplace(r, c, v);
                        }
                    }
                }
            }
        }
    }

    if (player_set == expected_set) {
        return AnswerResult::Correct;
    }

    // Check for partial correctness
    size_t correct_count = 0;
    for (const auto& p : player_set) {
        if (expected_set.contains(p)) {
            correct_count++;
        }
    }

    if (correct_count > 0 && correct_count < expected_set.size()) {
        return AnswerResult::PartiallyCorrect;
    }

    return AnswerResult::Incorrect;
}

std::string TrainingViewModel::buildFeedback(AnswerResult result, const SolveStep& expected) {
    switch (result) {
        case AnswerResult::Correct:
            return fmt::format("Correct! {}", expected.explanation);
        case AnswerResult::PartiallyCorrect:
            return fmt::format("Partially correct. {}", expected.explanation);
        case AnswerResult::Incorrect:
            return fmt::format("Not quite. {}", expected.explanation);
    }
    return "Unknown result.";
}

}  // namespace sudoku::viewmodel
