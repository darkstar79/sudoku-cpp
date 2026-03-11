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
#include "../core/i_training_statistics_manager.h"
#include "../core/observable.h"
#include "../core/technique_descriptions.h"
#include "../core/training_types.h"
#include "core/solve_step.h"
#include "core/solving_technique.h"

#include <format>
#include <memory>
#include <optional>
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
                      std::shared_ptr<core::ILocalizationManager> loc_manager,
                      std::shared_ptr<core::ITrainingStatisticsManager> stats_manager = nullptr);

    // --- Observable properties ---

    core::Observable<core::TrainingUIState> trainingState;
    core::Observable<core::TrainingBoard> trainingBoard;
    core::Observable<core::TrainingBoard> feedbackBoard;  ///< Board shown on feedback page with diff highlights
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

    /// Reveal the full expected solution on the feedback board
    void revealSolution();

    /// Record a board change from the view (pushes undo snapshot)
    void recordBoardChange(const core::TrainingBoard& board);

    /// Undo the last board modification
    void undo();

    /// Redo a previously undone board modification
    void redo();

    /// Whether undo is available
    [[nodiscard]] bool canUndo() const;

    /// Whether redo is available
    [[nodiscard]] bool canRedo() const;

    // --- Getters for current state ---

    /// Get the description for the currently selected technique
    [[nodiscard]] core::TechniqueDescription currentDescription() const;

    /// Get the current exercises (for testing/inspection)
    [[nodiscard]] const std::vector<core::TrainingExercise>& exercises() const {
        return exercises_;
    }

    /// Get the training statistics manager (for UI mastery badges)
    [[nodiscard]] std::shared_ptr<core::ITrainingStatisticsManager> statsManager() const {
        return stats_manager_;
    }

private:
    std::shared_ptr<core::ITrainingExerciseGenerator> exercise_generator_;
    std::shared_ptr<core::ILocalizationManager> loc_manager_;
    std::shared_ptr<core::ITrainingStatisticsManager> stats_manager_;

    // Exercise state
    std::vector<core::TrainingExercise> exercises_;
    int initial_step_count_{0};  ///< Number of valid steps when exercise loaded
    int found_step_count_{0};    ///< Number of steps the player has found so far

    // Undo/redo history (snapshot-based)
    static constexpr int kMaxUndoHistory = 50;
    std::vector<core::TrainingBoard> board_history_;
    int history_index_{-1};
    void pushSnapshot(const core::TrainingBoard& board);
    void clearHistory();

    /// Populate the training board from the current exercise
    void loadExerciseBoard();

    /// Result of answer evaluation: the verdict and the matched step (if any)
    struct EvalResult {
        core::AnswerResult result{core::AnswerResult::Incorrect};
        std::optional<core::SolveStep> matched_step;  ///< The step that matched, or nullopt
    };

    /// Evaluate a placement answer against all valid instances of the technique
    [[nodiscard]] EvalResult evaluatePlacement(const core::TrainingBoard& player_board,
                                               const core::TrainingExercise& exercise) const;

    /// Evaluate an elimination answer against all valid instances of the technique
    [[nodiscard]] EvalResult evaluateElimination(const core::TrainingBoard& player_board,
                                                 const core::TrainingExercise& exercise) const;

    /// Build feedback message based on result
    [[nodiscard]] static std::string buildFeedback(core::AnswerResult result, const core::SolveStep& step);

    /// Build a diff board highlighting correct/wrong/missed answers
    void buildDiffBoard(const core::TrainingBoard& player_board, const core::TrainingExercise& exercise,
                        const core::SolveStep& matched_step);

    /// Apply a correct step to the board and continue the exercise
    void applyContinue(const core::SolveStep& matched_step);

    /// Check if more valid steps remain for the current technique
    [[nodiscard]] bool hasMoreSteps() const;

    /// Record lesson stats when transitioning to LessonComplete
    void recordLessonStats();
};

}  // namespace sudoku::viewmodel
