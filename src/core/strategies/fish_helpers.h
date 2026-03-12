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

#include "../candidate_grid.h"
#include "../i_game_validator.h"
#include "../strategy_base.h"

#include <algorithm>
#include <ranges>
#include <vector>

namespace sudoku::core {

/// Shared utility functions for fish-based strategies (X-Wing, Swordfish, Jellyfish, and finned variants).
/// Inherits StrategyBase for cellIndex(), BOARD_SIZE, etc.
class FishHelpers : protected StrategyBase {
public:
    /// For each row (by_row=true) or column (by_row=false), collect the column/row indices
    /// where `value` is a candidate. Returns a vector of 9 vectors indexed by row/col.
    [[nodiscard]] static std::vector<std::vector<size_t>>
    collectCandidatePositions(const std::vector<std::vector<int>>& board, const CandidateGrid& candidates, int value,
                              bool by_row) {
        std::vector<std::vector<size_t>> positions(BOARD_SIZE);
        for (size_t primary = 0; primary < BOARD_SIZE; ++primary) {
            for (size_t secondary = 0; secondary < BOARD_SIZE; ++secondary) {
                size_t row = by_row ? primary : secondary;
                size_t col = by_row ? secondary : primary;
                if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, value)) {
                    positions[primary].push_back(secondary);
                }
            }
        }
        return positions;
    }

    /// Compute sorted union of an arbitrary number of index vectors.
    template <typename... Vecs>
    [[nodiscard]] static std::vector<size_t> indexUnion(const Vecs&... vecs) {
        std::vector<size_t> result;
        result.reserve(BOARD_SIZE);
        auto add = [&](const std::vector<size_t>& vec) {
            for (size_t idx : vec) {
                if (!std::ranges::contains(result, idx)) {
                    result.push_back(idx);
                }
            }
        };
        (add(vecs), ...);
        std::ranges::sort(result);
        return result;
    }
};

}  // namespace sudoku::core
