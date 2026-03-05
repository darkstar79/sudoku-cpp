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

#include "../../src/core/puzzle_rating.h"
#include "../../src/core/solving_technique.h"

#include <limits>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("PuzzleRating - Construction", "[puzzle_rating]") {
    SECTION("Creates rating with all fields") {
        std::vector<SolveStep> solve_path = {{.type = SolveStepType::Placement,
                                              .technique = SolvingTechnique::NakedSingle,
                                              .position = Position{.row = 0, .col = 0},
                                              .value = 5,
                                              .eliminations = {},
                                              .explanation = "Test",
                                              .points = 10,
                                              .explanation_data = {}},
                                             {.type = SolveStepType::Placement,
                                              .technique = SolvingTechnique::HiddenSingle,
                                              .position = Position{.row = 1, .col = 1},
                                              .value = 3,
                                              .eliminations = {},
                                              .explanation = "Test",
                                              .points = 15,
                                              .explanation_data = {}}};

        PuzzleRating rating{.total_score = 25,
                            .solve_path = solve_path,
                            .requires_backtracking = false,
                            .estimated_difficulty = Difficulty::Easy};

        REQUIRE(rating.total_score == 25);
        REQUIRE(rating.solve_path.size() == 2);
        REQUIRE_FALSE(rating.requires_backtracking);
        REQUIRE(rating.estimated_difficulty == Difficulty::Easy);
    }

    SECTION("Creates rating with backtracking") {
        PuzzleRating rating{.total_score = 1250,
                            .solve_path = {},
                            .requires_backtracking = true,
                            .estimated_difficulty = Difficulty::Expert};

        REQUIRE(rating.total_score == 1250);
        REQUIRE(rating.requires_backtracking);
        REQUIRE(rating.estimated_difficulty == Difficulty::Expert);
    }
}

TEST_CASE("PuzzleRating - Equality Comparison", "[puzzle_rating]") {
    SECTION("Equal ratings are equal") {
        PuzzleRating rating1{.total_score = 100,
                             .solve_path = {},
                             .requires_backtracking = false,
                             .estimated_difficulty = Difficulty::Medium};

        PuzzleRating rating2{.total_score = 100,
                             .solve_path = {},
                             .requires_backtracking = false,
                             .estimated_difficulty = Difficulty::Medium};

        REQUIRE(rating1 == rating2);
    }

    SECTION("Different scores are not equal") {
        PuzzleRating rating1{.total_score = 100,
                             .solve_path = {},
                             .requires_backtracking = false,
                             .estimated_difficulty = Difficulty::Medium};
        PuzzleRating rating2{.total_score = 200,
                             .solve_path = {},
                             .requires_backtracking = false,
                             .estimated_difficulty = Difficulty::Medium};

        REQUIRE_FALSE(rating1 == rating2);
    }
}

TEST_CASE("ratingToDifficulty - Converts rating to difficulty", "[puzzle_rating]") {
    SECTION("Easy range (0-750)") {
        REQUIRE(ratingToDifficulty(0) == Difficulty::Easy);
        REQUIRE(ratingToDifficulty(375) == Difficulty::Easy);
        REQUIRE(ratingToDifficulty(749) == Difficulty::Easy);
    }

    SECTION("Medium range (750-1250)") {
        REQUIRE(ratingToDifficulty(750) == Difficulty::Medium);
        REQUIRE(ratingToDifficulty(1000) == Difficulty::Medium);
        REQUIRE(ratingToDifficulty(1249) == Difficulty::Medium);
    }

    SECTION("Hard range (1250-1750)") {
        REQUIRE(ratingToDifficulty(1250) == Difficulty::Hard);
        REQUIRE(ratingToDifficulty(1500) == Difficulty::Hard);
        REQUIRE(ratingToDifficulty(1749) == Difficulty::Hard);
    }

    SECTION("Expert range (1750-2250)") {
        REQUIRE(ratingToDifficulty(1750) == Difficulty::Expert);
        REQUIRE(ratingToDifficulty(2000) == Difficulty::Expert);
        REQUIRE(ratingToDifficulty(2249) == Difficulty::Expert);
    }

    SECTION("Master range (2250+)") {
        REQUIRE(ratingToDifficulty(2250) == Difficulty::Master);
        REQUIRE(ratingToDifficulty(3000) == Difficulty::Master);
        REQUIRE(ratingToDifficulty(5000) == Difficulty::Master);
    }

    SECTION("Boundary values") {
        REQUIRE(ratingToDifficulty(0) == Difficulty::Easy);
        REQUIRE(ratingToDifficulty(750) == Difficulty::Medium);
        REQUIRE(ratingToDifficulty(1250) == Difficulty::Hard);
        REQUIRE(ratingToDifficulty(1750) == Difficulty::Expert);
        REQUIRE(ratingToDifficulty(2250) == Difficulty::Master);
    }
}

TEST_CASE("getDifficultyRatingRange - Returns min/max rating for difficulty", "[puzzle_rating]") {
    SECTION("Easy difficulty range") {
        auto [min_rating, max_rating] = getDifficultyRatingRange(Difficulty::Easy);

        REQUIRE(min_rating == 0);
        REQUIRE(max_rating == 750);
        REQUIRE(min_rating < max_rating);
    }

    SECTION("Medium difficulty range") {
        auto [min_rating, max_rating] = getDifficultyRatingRange(Difficulty::Medium);

        REQUIRE(min_rating == 750);
        REQUIRE(max_rating == 1250);
        REQUIRE(min_rating < max_rating);
    }

    SECTION("Hard difficulty range") {
        auto [min_rating, max_rating] = getDifficultyRatingRange(Difficulty::Hard);

        REQUIRE(min_rating == 1250);
        REQUIRE(max_rating == 1750);
        REQUIRE(min_rating < max_rating);
    }

    SECTION("Expert difficulty range") {
        auto [min_rating, max_rating] = getDifficultyRatingRange(Difficulty::Expert);

        REQUIRE(min_rating == 1750);
        REQUIRE(max_rating == 2250);
        REQUIRE(min_rating < max_rating);
    }

    SECTION("Master difficulty range") {
        auto [min_rating, max_rating] = getDifficultyRatingRange(Difficulty::Master);

        REQUIRE(min_rating == 2250);
        REQUIRE(max_rating == std::numeric_limits<int>::max());
        REQUIRE(min_rating < max_rating);
    }
}

TEST_CASE("getDifficultyRatingRange - Range boundaries are consistent", "[puzzle_rating]") {
    SECTION("No gaps between difficulty ranges") {
        auto [easy_min, easy_max] = getDifficultyRatingRange(Difficulty::Easy);
        auto [medium_min, medium_max] = getDifficultyRatingRange(Difficulty::Medium);
        auto [hard_min, hard_max] = getDifficultyRatingRange(Difficulty::Hard);
        auto [expert_min, expert_max] = getDifficultyRatingRange(Difficulty::Expert);
        auto [master_min, master_max] = getDifficultyRatingRange(Difficulty::Master);

        // Verify continuous ranges (no gaps)
        REQUIRE(easy_max == medium_min);
        REQUIRE(medium_max == hard_min);
        REQUIRE(hard_max == expert_min);
        REQUIRE(expert_max == master_min);
    }

    SECTION("Rating to difficulty conversion is consistent with ranges") {
        // Test all difficulty levels
        for (auto difficulty :
             {Difficulty::Easy, Difficulty::Medium, Difficulty::Hard, Difficulty::Expert, Difficulty::Master}) {
            auto [min_rating, max_rating] = getDifficultyRatingRange(difficulty);

            // A rating at the minimum should map to this difficulty
            REQUIRE(ratingToDifficulty(min_rating) == difficulty);

            // A rating just below max should map to this difficulty
            if (max_rating > 0) {
                REQUIRE(ratingToDifficulty(max_rating - 1) == difficulty);
            }
        }
    }
}

TEST_CASE("ratingToDifficulty - Round-trip consistency", "[puzzle_rating]") {
    SECTION("Rating within range maps back to same difficulty") {
        // Test representative ratings for each difficulty
        struct TestCase {
            int rating;
            Difficulty expected_difficulty;
        };

        std::vector<TestCase> test_cases = {{100, Difficulty::Easy},    {400, Difficulty::Easy},
                                            {800, Difficulty::Medium},  {1100, Difficulty::Medium},
                                            {1300, Difficulty::Hard},   {1500, Difficulty::Hard},
                                            {1800, Difficulty::Expert}, {2100, Difficulty::Expert},
                                            {2300, Difficulty::Master}, {3000, Difficulty::Master}};

        for (const auto& test_case : test_cases) {
            auto difficulty = ratingToDifficulty(test_case.rating);
            REQUIRE(difficulty == test_case.expected_difficulty);

            // Verify rating is within expected range
            auto [min_rating, max_rating] = getDifficultyRatingRange(difficulty);
            REQUIRE(test_case.rating >= min_rating);
            REQUIRE(test_case.rating < max_rating);
        }
    }
}

TEST_CASE("PuzzleRating - Score Calculation", "[puzzle_rating]") {
    SECTION("Total score is sum of technique points") {
        std::vector<SolveStep> solve_path = {{.type = SolveStepType::Placement,
                                              .technique = SolvingTechnique::NakedSingle,
                                              .position = {},
                                              .value = 1,
                                              .eliminations = {},
                                              .explanation = "",
                                              .points = 10,
                                              .explanation_data = {}},
                                             {.type = SolveStepType::Placement,
                                              .technique = SolvingTechnique::NakedSingle,
                                              .position = {},
                                              .value = 2,
                                              .eliminations = {},
                                              .explanation = "",
                                              .points = 10,
                                              .explanation_data = {}},
                                             {.type = SolveStepType::Placement,
                                              .technique = SolvingTechnique::HiddenSingle,
                                              .position = {},
                                              .value = 3,
                                              .eliminations = {},
                                              .explanation = "",
                                              .points = 15,
                                              .explanation_data = {}},
                                             {.type = SolveStepType::Placement,
                                              .technique = SolvingTechnique::NakedPair,
                                              .position = {},
                                              .value = 4,
                                              .eliminations = {},
                                              .explanation = "",
                                              .points = 50,
                                              .explanation_data = {}}};

        // Sum: 10 + 10 + 15 + 50 = 85
        PuzzleRating rating{.total_score = 85,
                            .solve_path = solve_path,
                            .requires_backtracking = false,
                            .estimated_difficulty = Difficulty::Easy};

        // Verify sum matches total_score
        int calculated_sum = 0;
        for (const auto& step : rating.solve_path) {
            calculated_sum += step.points;
        }

        REQUIRE(rating.total_score == calculated_sum);
    }

    SECTION("Backtracking step has 750 points but is excluded from rating score") {
        std::vector<SolveStep> solve_path = {{.type = SolveStepType::Placement,
                                              .technique = SolvingTechnique::NakedSingle,
                                              .position = {},
                                              .value = 1,
                                              .eliminations = {},
                                              .explanation = "",
                                              .points = 10,
                                              .explanation_data = {}},
                                             {.type = SolveStepType::Placement,
                                              .technique = SolvingTechnique::Backtracking,
                                              .position = {},
                                              .value = 2,
                                              .eliminations = {},
                                              .explanation = "",
                                              .points = 750,
                                              .explanation_data = {}}};

        // Rater excludes backtracking from total_score (only logical techniques count)
        PuzzleRating rating{.total_score = 10,
                            .solve_path = solve_path,
                            .requires_backtracking = true,
                            .estimated_difficulty = Difficulty::Expert};

        // Step itself has 750 points, but rating total excludes it
        REQUIRE(solve_path[1].points == 750);
        REQUIRE(rating.total_score == 10);
        REQUIRE(rating.requires_backtracking);
    }
}
