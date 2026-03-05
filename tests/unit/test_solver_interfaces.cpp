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

#include "../../src/core/i_puzzle_rater.h"
#include "../../src/core/i_solving_strategy.h"
#include "../../src/core/i_sudoku_solver.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

// Mock implementation for testing interface contracts
class MockStrategy : public ISolvingStrategy {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const std::vector<std::vector<int>>&,
                                                    const CandidateGrid&) const override {
        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::NakedSingle;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Mock Strategy";
    }

    [[nodiscard]] int getDifficultyPoints() const override {
        return 10;
    }
};

class MockSolver : public ISudokuSolver {
public:
    [[nodiscard]] std::expected<SolveStep, SolverError>
    findNextStep(const std::vector<std::vector<int>>&) const override {
        return std::unexpected(SolverError::Unsolvable);
    }

    [[nodiscard]] std::expected<SolverResult, SolverError>
    solvePuzzle(const std::vector<std::vector<int>>&) const override {
        return std::unexpected(SolverError::Unsolvable);
    }

    [[nodiscard]] bool applyStep(std::vector<std::vector<int>>&, const SolveStep&) const override {
        return false;
    }
};

class MockRater : public IPuzzleRater {
public:
    [[nodiscard]] std::expected<PuzzleRating, RatingError>
    ratePuzzle(const std::vector<std::vector<int>>&) const override {
        return std::unexpected(RatingError::Unsolvable);
    }
};

TEST_CASE("ISolvingStrategy - Interface Contract", "[solver_interfaces]") {
    SECTION("Can instantiate mock implementation") {
        MockStrategy strategy;

        REQUIRE(strategy.getTechnique() == SolvingTechnique::NakedSingle);
        REQUIRE(strategy.getName() == "Mock Strategy");
        REQUIRE(strategy.getDifficultyPoints() == 10);
    }

    SECTION("findStep returns optional") {
        MockStrategy strategy;
        std::vector<std::vector<int>> board(9, std::vector<int>(9, 0));
        CandidateGrid state(board);

        auto result = strategy.findStep(board, state);

        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Can use polymorphically") {
        std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<MockStrategy>();

        REQUIRE(strategy->getTechnique() == SolvingTechnique::NakedSingle);
        REQUIRE(strategy->getDifficultyPoints() == 10);
    }
}

TEST_CASE("ISudokuSolver - Interface Contract", "[solver_interfaces]") {
    SECTION("Can instantiate mock implementation") {
        MockSolver solver;

        std::vector<std::vector<int>> board(9, std::vector<int>(9, 0));
        auto result = solver.findNextStep(board);

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == SolverError::Unsolvable);
    }

    SECTION("solvePuzzle returns expected") {
        MockSolver solver;
        std::vector<std::vector<int>> board(9, std::vector<int>(9, 0));

        auto result = solver.solvePuzzle(board);

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == SolverError::Unsolvable);
    }

    SECTION("applyStep returns bool") {
        MockSolver solver;
        std::vector<std::vector<int>> board(9, std::vector<int>(9, 0));
        SolveStep step{.type = SolveStepType::Placement,
                       .technique = SolvingTechnique::NakedSingle,
                       .position = Position{.row = 0, .col = 0},
                       .value = 5,
                       .eliminations = {},
                       .explanation = "Test",
                       .points = 10,
                       .explanation_data = {}};

        bool result = solver.applyStep(board, step);

        REQUIRE_FALSE(result);
    }

    SECTION("Can use polymorphically") {
        std::unique_ptr<ISudokuSolver> solver = std::make_unique<MockSolver>();

        std::vector<std::vector<int>> board(9, std::vector<int>(9, 0));
        auto result = solver->findNextStep(board);

        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("IPuzzleRater - Interface Contract", "[solver_interfaces]") {
    SECTION("Can instantiate mock implementation") {
        MockRater rater;

        std::vector<std::vector<int>> board(9, std::vector<int>(9, 0));
        auto result = rater.ratePuzzle(board);

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == RatingError::Unsolvable);
    }

    SECTION("Can use polymorphically") {
        std::unique_ptr<IPuzzleRater> rater = std::make_unique<MockRater>();

        std::vector<std::vector<int>> board(9, std::vector<int>(9, 0));
        auto result = rater->ratePuzzle(board);

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == RatingError::Unsolvable);
    }
}

TEST_CASE("SolverError - Enum Values", "[solver_interfaces]") {
    SECTION("All error types are defined") {
        auto invalid = SolverError::InvalidBoard;
        auto unsolvable = SolverError::Unsolvable;
        auto timeout = SolverError::Timeout;

        REQUIRE(invalid != unsolvable);
        REQUIRE(invalid != timeout);
        REQUIRE(unsolvable != timeout);
    }

    SECTION("Underlying type is uint8_t") {
        REQUIRE(sizeof(SolverError) == sizeof(uint8_t));
    }
}

TEST_CASE("RatingError - Enum Values", "[solver_interfaces]") {
    SECTION("All error types are defined") {
        auto invalid = RatingError::InvalidBoard;
        auto unsolvable = RatingError::Unsolvable;
        auto timeout = RatingError::Timeout;

        REQUIRE(invalid != unsolvable);
        REQUIRE(invalid != timeout);
        REQUIRE(unsolvable != timeout);
    }

    SECTION("Underlying type is uint8_t") {
        REQUIRE(sizeof(RatingError) == sizeof(uint8_t));
    }
}

TEST_CASE("SolverResult - Construction and Equality", "[solver_interfaces]") {
    SECTION("Creates solver result with all fields") {
        std::vector<std::vector<int>> solution(9, std::vector<int>(9, 1));
        std::vector<SolveStep> solve_path = {{.type = SolveStepType::Placement,
                                              .technique = SolvingTechnique::NakedSingle,
                                              .position = Position{.row = 0, .col = 0},
                                              .value = 5,
                                              .eliminations = {},
                                              .explanation = "Test",
                                              .points = 10,
                                              .explanation_data = {}}};

        SolverResult result{.solution = solution, .solve_path = solve_path, .used_backtracking = false};

        REQUIRE(result.solution.size() == 9);
        REQUIRE(result.solve_path.size() == 1);
        REQUIRE_FALSE(result.used_backtracking);
    }

    SECTION("Equal results are equal") {
        std::vector<std::vector<int>> solution(9, std::vector<int>(9, 1));

        SolverResult result1{.solution = solution, .solve_path = {}, .used_backtracking = false};
        SolverResult result2{.solution = solution, .solve_path = {}, .used_backtracking = false};

        REQUIRE(result1 == result2);
    }

    SECTION("Different backtracking flag makes results not equal") {
        std::vector<std::vector<int>> solution(9, std::vector<int>(9, 1));

        SolverResult result1{.solution = solution, .solve_path = {}, .used_backtracking = false};
        SolverResult result2{.solution = solution, .solve_path = {}, .used_backtracking = true};

        REQUIRE_FALSE(result1 == result2);
    }
}
