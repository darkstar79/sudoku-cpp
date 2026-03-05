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

#include "i_puzzle_generator.h"  // For Difficulty enum
#include "solve_step.h"

#include <limits>
#include <utility>
#include <vector>

namespace sudoku::core {

/// Complete rating information for a Sudoku puzzle
struct PuzzleRating {
    int total_score;                    ///< Sum of technique points (Sudoku Explainer scale)
    std::vector<SolveStep> solve_path;  ///< All steps required to solve puzzle
    bool requires_backtracking;         ///< True if logical techniques alone insufficient
    Difficulty estimated_difficulty;    ///< Difficulty level based on rating

    bool operator==(const PuzzleRating& other) const = default;
};

/// Returns the rating range [min, max) for a difficulty level
/// @param difficulty Difficulty level
/// @return Pair of (min_rating, max_rating) - inclusive min, exclusive max
/// @note Single source of truth for difficulty thresholds
[[nodiscard]] constexpr std::pair<int, int> getDifficultyRatingRange(Difficulty difficulty) {
    switch (difficulty) {
        case Difficulty::Easy:
            return {0, 750};
        case Difficulty::Medium:
            return {750, 1250};
        case Difficulty::Hard:
            return {1250, 1750};
        case Difficulty::Expert:
            return {1750, 2250};
        case Difficulty::Master:
            return {2250, std::numeric_limits<int>::max()};
    }
    return {0, 0};  // Unreachable, but silences compiler warning
}

/// Converts rating score to difficulty level
/// @param rating Total Sudoku Explainer rating score
/// @return Estimated difficulty level
[[nodiscard]] constexpr Difficulty ratingToDifficulty(int rating) {
    if (rating >= getDifficultyRatingRange(Difficulty::Master).first) {
        return Difficulty::Master;
    }
    if (rating >= getDifficultyRatingRange(Difficulty::Expert).first) {
        return Difficulty::Expert;
    }
    if (rating >= getDifficultyRatingRange(Difficulty::Hard).first) {
        return Difficulty::Hard;
    }
    if (rating >= getDifficultyRatingRange(Difficulty::Medium).first) {
        return Difficulty::Medium;
    }
    return Difficulty::Easy;
}

}  // namespace sudoku::core
