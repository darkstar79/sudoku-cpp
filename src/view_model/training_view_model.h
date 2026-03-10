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

#include "../core/i_localization_manager.h"
#include "../core/i_training_exercise_generator.h"
#include "../core/observable.h"
#include "../core/technique_descriptions.h"
#include "../core/training_types.h"
#include "core/solve_step.h"
#include "core/solving_technique.h"

#include <format>
#include <memory>
#include <string>
#include <vector>

namespace sudoku::viewmodel {

/// ViewModel for Training Mode — manages state machine, exercises, and answer evaluation
class TrainingViewModel {
public:
    /// Constructor
    /// @param exercise_generator Exercise generation engine
    /// @param loc_manager Localization manager for UI strings
    TrainingViewModel(std::shared_ptr<core::ITrainingExerciseGenerator> exercise_generator,
                      std::shared_ptr<core::ILocalizationManager> loc_manager);

    // --- Observable properties ---

    core::Observable<core::TrainingUIState> trainingState;
    core::Observable<core::TrainingBoard> trainingBoard;
    core::Observable<std::string> errorMessage;

    // --- Public actions ---

    /// Select a technique to practice (TechniqueSelection → TheoryReview)
    void selectTechnique(core::SolvingTechnique technique);

    /// Start exercises after reading theory (TheoryReview → Exercise)
    void startExercises();

    /// Submit the player's answer for evaluation (Exercise → Feedback)
    void submitAnswer(const core::TrainingBoard& player_board);

    /// Request a progressive hint (up to 3 levels)
    void requestHint();

    /// Skip current exercise (advance without scoring)
    void skipExercise();

    /// Retry current exercise (reset player selections)
    void retryExercise();

    /// Advance to next exercise after feedback (Feedback → Exercise or LessonComplete)
    void nextExercise();

    /// Return to technique selection from any phase
    void returnToSelection();

    // --- Getters for current state ---

    /// Get the description for the currently selected technique
    [[nodiscard]] core::TechniqueDescription currentDescription() const;

    /// Get the current exercises (for testing/inspection)
    [[nodiscard]] const std::vector<core::TrainingExercise>& exercises() const {
        return exercises_;
    }

private:
    std::shared_ptr<core::ITrainingExerciseGenerator> exercise_generator_;
    std::shared_ptr<core::ILocalizationManager> loc_manager_;

    // Exercise state
    std::vector<core::TrainingExercise> exercises_;

    /// Populate the training board from the current exercise
    void loadExerciseBoard();

    /// Evaluate a placement answer
    [[nodiscard]] static core::AnswerResult evaluatePlacement(const core::TrainingBoard& player_board,
                                                              const core::SolveStep& expected);

    /// Evaluate an elimination answer
    [[nodiscard]] core::AnswerResult evaluateElimination(const core::TrainingBoard& player_board,
                                                         const core::SolveStep& expected) const;

    /// Build feedback message based on result
    [[nodiscard]] static std::string buildFeedback(core::AnswerResult result, const core::SolveStep& expected);
};

}  // namespace sudoku::viewmodel
