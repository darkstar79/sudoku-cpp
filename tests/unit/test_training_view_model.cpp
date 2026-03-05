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

#include "../../src/core/i_localization_manager.h"
#include "../../src/core/i_training_exercise_generator.h"
#include "../../src/view_model/training_view_model.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace sudoku::viewmodel;

// --- Mock Exercise Generator ---

namespace {

/// Creates a predictable set of exercises without actual puzzle generation
class MockExerciseGenerator : public ITrainingExerciseGenerator {
public:
    mutable SolvingTechnique last_requested_technique{SolvingTechnique::NakedSingle};
    mutable int last_requested_count{0};
    bool should_fail{false};
    std::string fail_message{"Mock generation failed"};

    [[nodiscard]] std::expected<std::vector<TrainingExercise>, std::string>
    generateExercises(SolvingTechnique technique, int count) const override {
        last_requested_technique = technique;
        last_requested_count = count;

        if (should_fail) {
            return std::unexpected(fail_message);
        }

        std::vector<TrainingExercise> exercises;
        for (int i = 0; i < count; ++i) {
            TrainingExercise ex;
            ex.board = std::vector<std::vector<int>>(9, std::vector<int>(9, 0));
            // Place some givens
            ex.board[0][0] = 5;
            ex.board[0][1] = 3;

            // Set up candidate masks (81 entries)
            ex.candidate_masks.resize(81, 0);
            // Cell (0,2) has candidates {1, 7} = bits 1 and 7 = 0x0082
            ex.candidate_masks[2] = 0x0082;
            // Cell (1,0) has candidate {9} only = bit 9 = 0x0200
            ex.candidate_masks[9] = 0x0200;

            if (technique == SolvingTechnique::NakedSingle || technique == SolvingTechnique::HiddenSingle) {
                ex.expected_step = SolveStep{
                    .type = SolveStepType::Placement,
                    .technique = technique,
                    .position = Position{.row = 1, .col = 0},
                    .value = 9,
                    .eliminations = {},
                    .explanation = "Naked Single at R2C1: only value 9 is possible",
                    .points = getTechniquePoints(technique),
                    .explanation_data = {.positions = {Position{.row = 1, .col = 0}}, .values = {9}},
                };
                ex.interaction_mode = TrainingInteractionMode::Placement;
            } else {
                ex.expected_step = SolveStep{
                    .type = SolveStepType::Elimination,
                    .technique = technique,
                    .position = Position{.row = 0, .col = 0},
                    .value = 0,
                    .eliminations = {{Position{.row = 0, .col = 2}, 1}},
                    .explanation = "Technique eliminates 1 from R1C3",
                    .points = getTechniquePoints(technique),
                    .explanation_data = {.positions = {Position{.row = 0, .col = 2}}, .values = {1}},
                };
                ex.interaction_mode = TrainingInteractionMode::Elimination;
            }

            ex.technique = technique;
            ex.is_guided_coloring = false;
            exercises.push_back(std::move(ex));
        }
        return exercises;
    }
};

/// Minimal localization manager for tests
class MockLocManager : public ILocalizationManager {
public:
    [[nodiscard]] const char* getString(std::string_view key) const override {
        return key.data();
    }
    [[nodiscard]] std::expected<void, std::string> setLocale(std::string_view /*locale_code*/) override {
        return {};
    }
    [[nodiscard]] std::string_view getCurrentLocale() const override {
        return "en";
    }
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> getAvailableLocales() const override {
        return {{"en", "English"}};
    }
};

struct VMFixture {
    std::shared_ptr<MockExerciseGenerator> mock_gen = std::make_shared<MockExerciseGenerator>();
    std::shared_ptr<MockLocManager> mock_loc = std::make_shared<MockLocManager>();
    TrainingViewModel vm{mock_gen, mock_loc};
};

}  // namespace

// --- State machine transitions ---

TEST_CASE("TrainingViewModel - Initial State", "[training_view_model]") {
    VMFixture f;

    SECTION("Starts in TechniqueSelection phase") {
        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::TechniqueSelection);
    }
}

TEST_CASE("TrainingViewModel - selectTechnique", "[training_view_model]") {
    VMFixture f;

    SECTION("Transitions to TheoryReview") {
        f.vm.selectTechnique(SolvingTechnique::NakedPair);

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::TheoryReview);
        REQUIRE(f.vm.trainingState.get().current_technique == SolvingTechnique::NakedPair);
    }

    SECTION("Resets counters") {
        // First do a full cycle to set some state
        f.vm.selectTechnique(SolvingTechnique::NakedSingle);
        f.vm.startExercises();

        // Now select a new technique
        f.vm.selectTechnique(SolvingTechnique::NakedPair);

        REQUIRE(f.vm.trainingState.get().exercise_index == 0);
        REQUIRE(f.vm.trainingState.get().correct_count == 0);
        REQUIRE(f.vm.trainingState.get().hints_used == 0);
    }

    SECTION("Rejects Backtracking") {
        f.vm.selectTechnique(SolvingTechnique::Backtracking);

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::TechniqueSelection);
        REQUIRE_FALSE(f.vm.errorMessage.get().empty());
    }

    SECTION("Populates theory text") {
        f.vm.selectTechnique(SolvingTechnique::XWing);

        auto desc = f.vm.currentDescription();
        REQUIRE(desc.title == "X-Wing");
        REQUIRE_FALSE(desc.what_it_is.empty());
    }
}

TEST_CASE("TrainingViewModel - startExercises", "[training_view_model]") {
    VMFixture f;

    SECTION("Transitions to Exercise phase") {
        f.vm.selectTechnique(SolvingTechnique::NakedSingle);
        f.vm.startExercises();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::Exercise);
        REQUIRE(f.vm.trainingState.get().exercise_index == 0);
    }

    SECTION("Calls generator with correct technique") {
        f.vm.selectTechnique(SolvingTechnique::HiddenPair);
        f.vm.startExercises();

        REQUIRE(f.mock_gen->last_requested_technique == SolvingTechnique::HiddenPair);
        REQUIRE(f.mock_gen->last_requested_count == 5);
    }

    SECTION("Populates training board") {
        f.vm.selectTechnique(SolvingTechnique::NakedSingle);
        f.vm.startExercises();

        const auto& board = f.vm.trainingBoard.get();
        REQUIRE(board[0][0].value == 5);
        REQUIRE(board[0][0].is_given == true);
        REQUIRE(board[0][1].value == 3);
    }

    SECTION("Sets candidate lists from masks") {
        f.vm.selectTechnique(SolvingTechnique::NakedSingle);
        f.vm.startExercises();

        const auto& board = f.vm.trainingBoard.get();
        // Cell (1,0) has candidate {9} only
        REQUIRE(board[1][0].candidates.size() == 1);
        REQUIRE(board[1][0].candidates[0] == 9);
    }

    SECTION("Does nothing outside TheoryReview phase") {
        // Still in TechniqueSelection
        f.vm.startExercises();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::TechniqueSelection);
    }

    SECTION("Handles generator failure") {
        f.mock_gen->should_fail = true;
        f.vm.selectTechnique(SolvingTechnique::NakedSingle);
        f.vm.startExercises();

        // Should stay in TheoryReview since generation failed
        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::TheoryReview);
        REQUIRE_FALSE(f.vm.errorMessage.get().empty());
    }
}

TEST_CASE("TrainingViewModel - submitAnswer", "[training_view_model]") {
    VMFixture f;
    f.vm.selectTechnique(SolvingTechnique::NakedSingle);
    f.vm.startExercises();

    SECTION("Correct placement transitions to Feedback") {
        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 9;  // Expected placement

        f.vm.submitAnswer(board);

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::Feedback);
        REQUIRE(f.vm.trainingState.get().last_result == AnswerResult::Correct);
        REQUIRE(f.vm.trainingState.get().correct_count == 1);
    }

    SECTION("Wrong placement gives Incorrect") {
        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 5;  // Wrong value

        f.vm.submitAnswer(board);

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::Feedback);
        REQUIRE(f.vm.trainingState.get().last_result == AnswerResult::Incorrect);
        REQUIRE(f.vm.trainingState.get().correct_count == 0);
    }

    SECTION("Feedback message contains explanation") {
        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 9;

        f.vm.submitAnswer(board);

        REQUIRE(f.vm.trainingState.get().feedback_message.find("Naked Single") != std::string::npos);
    }
}

TEST_CASE("TrainingViewModel - requestHint", "[training_view_model]") {
    VMFixture f;
    f.vm.selectTechnique(SolvingTechnique::NakedSingle);
    f.vm.startExercises();

    SECTION("Increments hint level") {
        f.vm.requestHint();
        REQUIRE(f.vm.trainingState.get().current_hint_level == 1);
        REQUIRE(f.vm.trainingState.get().hints_used == 1);

        f.vm.requestHint();
        REQUIRE(f.vm.trainingState.get().current_hint_level == 2);
        REQUIRE(f.vm.trainingState.get().hints_used == 2);

        f.vm.requestHint();
        REQUIRE(f.vm.trainingState.get().current_hint_level == 3);
        REQUIRE(f.vm.trainingState.get().hints_used == 3);
    }

    SECTION("Caps at level 3") {
        f.vm.requestHint();
        f.vm.requestHint();
        f.vm.requestHint();
        f.vm.requestHint();  // Should be no-op

        REQUIRE(f.vm.trainingState.get().current_hint_level == 3);
        REQUIRE(f.vm.trainingState.get().hints_used == 3);
    }
}

TEST_CASE("TrainingViewModel - skipExercise", "[training_view_model]") {
    VMFixture f;
    f.vm.selectTechnique(SolvingTechnique::NakedSingle);
    f.vm.startExercises();

    SECTION("Advances to next exercise") {
        f.vm.skipExercise();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::Exercise);
        REQUIRE(f.vm.trainingState.get().exercise_index == 1);
    }

    SECTION("Does not change score") {
        f.vm.skipExercise();

        REQUIRE(f.vm.trainingState.get().correct_count == 0);
    }

    SECTION("Resets hint level") {
        f.vm.requestHint();
        f.vm.skipExercise();

        REQUIRE(f.vm.trainingState.get().current_hint_level == 0);
    }
}

TEST_CASE("TrainingViewModel - retryExercise", "[training_view_model]") {
    VMFixture f;
    f.vm.selectTechnique(SolvingTechnique::NakedSingle);
    f.vm.startExercises();

    SECTION("Reloads same exercise from Exercise phase") {
        f.vm.requestHint();
        f.vm.retryExercise();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::Exercise);
        REQUIRE(f.vm.trainingState.get().exercise_index == 0);
        REQUIRE(f.vm.trainingState.get().current_hint_level == 0);
    }

    SECTION("Works from Feedback phase") {
        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 5;  // Wrong answer
        f.vm.submitAnswer(board);
        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::Feedback);

        f.vm.retryExercise();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::Exercise);
        REQUIRE(f.vm.trainingState.get().exercise_index == 0);
    }
}

TEST_CASE("TrainingViewModel - nextExercise", "[training_view_model]") {
    VMFixture f;
    f.vm.selectTechnique(SolvingTechnique::NakedSingle);
    f.vm.startExercises();

    SECTION("Advances from Feedback to next Exercise") {
        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 9;
        f.vm.submitAnswer(board);

        f.vm.nextExercise();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::Exercise);
        REQUIRE(f.vm.trainingState.get().exercise_index == 1);
    }

    SECTION("Transitions to LessonComplete on last exercise") {
        // Skip through all exercises via submit+next
        for (int i = 0; i < 5; ++i) {
            auto board = f.vm.trainingBoard.get();
            board[1][0].value = 9;
            f.vm.submitAnswer(board);
            if (i < 4) {
                f.vm.nextExercise();
            }
        }

        f.vm.nextExercise();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::LessonComplete);
    }

    SECTION("Does nothing outside Feedback phase") {
        // Still in Exercise phase
        f.vm.nextExercise();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::Exercise);
        REQUIRE(f.vm.trainingState.get().exercise_index == 0);
    }
}

TEST_CASE("TrainingViewModel - returnToSelection", "[training_view_model]") {
    VMFixture f;

    SECTION("Returns from TheoryReview") {
        f.vm.selectTechnique(SolvingTechnique::NakedPair);
        f.vm.returnToSelection();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::TechniqueSelection);
    }

    SECTION("Returns from Exercise") {
        f.vm.selectTechnique(SolvingTechnique::NakedSingle);
        f.vm.startExercises();
        f.vm.returnToSelection();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::TechniqueSelection);
    }

    SECTION("Returns from LessonComplete") {
        f.vm.selectTechnique(SolvingTechnique::NakedSingle);
        f.vm.startExercises();

        // Complete all exercises
        for (int i = 0; i < 5; ++i) {
            auto board = f.vm.trainingBoard.get();
            board[1][0].value = 9;
            f.vm.submitAnswer(board);
            if (i < 4) {
                f.vm.nextExercise();
            }
        }
        f.vm.nextExercise();
        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::LessonComplete);

        f.vm.returnToSelection();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::TechniqueSelection);
    }

    SECTION("Clears exercises") {
        f.vm.selectTechnique(SolvingTechnique::NakedSingle);
        f.vm.startExercises();
        REQUIRE_FALSE(f.vm.exercises().empty());

        f.vm.returnToSelection();

        REQUIRE(f.vm.exercises().empty());
    }
}

TEST_CASE("TrainingViewModel - Observable notifications", "[training_view_model]") {
    VMFixture f;

    SECTION("trainingState fires on selectTechnique") {
        int notify_count = 0;
        f.vm.trainingState.subscribe([&notify_count](const TrainingUIState&) { notify_count++; });

        f.vm.selectTechnique(SolvingTechnique::NakedPair);

        REQUIRE(notify_count == 1);
    }

    SECTION("trainingBoard fires on startExercises") {
        int notify_count = 0;
        f.vm.trainingBoard.subscribe([&notify_count](const TrainingBoard&) { notify_count++; });

        f.vm.selectTechnique(SolvingTechnique::NakedSingle);
        f.vm.startExercises();

        REQUIRE(notify_count >= 1);
    }
}

TEST_CASE("TrainingViewModel - Score tracking", "[training_view_model]") {
    VMFixture f;
    f.vm.selectTechnique(SolvingTechnique::NakedSingle);
    f.vm.startExercises();

    SECTION("Correct answers increment score") {
        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 9;
        f.vm.submitAnswer(board);

        REQUIRE(f.vm.trainingState.get().correct_count == 1);

        f.vm.nextExercise();
        board = f.vm.trainingBoard.get();
        board[1][0].value = 9;
        f.vm.submitAnswer(board);

        REQUIRE(f.vm.trainingState.get().correct_count == 2);
    }

    SECTION("Incorrect answers do not increment score") {
        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 5;
        f.vm.submitAnswer(board);

        REQUIRE(f.vm.trainingState.get().correct_count == 0);
    }
}
