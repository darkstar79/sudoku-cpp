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

#include <array>
#include <vector>

namespace sudoku::core {

/// Shared utility functions for coloring-based strategies (Simple Coloring, Multi-Coloring, X-Cycles).
/// Inherits StrategyBase for cellIndex(), sees(), etc.
class ColoringHelpers : protected StrategyBase {
public:
    /// Flat index for a 9x9 grid
    [[nodiscard]] static size_t cellIndex(size_t row, size_t col) {
        return (row * BOARD_SIZE) + col;
    }

    /// Build adjacency list of conjugate pairs for a single value.
    /// Two cells are linked if they are the only two candidates for `value` in a row, column, or box.
    [[nodiscard]] static std::array<std::vector<size_t>, TOTAL_CELLS>
    buildConjugatePairGraph(const std::vector<std::vector<int>>& board, const CandidateGrid& candidates, int value) {
        std::array<std::vector<size_t>, TOTAL_CELLS> adj{};

        auto addEdge = [&adj](size_t a, size_t b) {
            adj[a].push_back(b);
            adj[b].push_back(a);
        };

        // Rows
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            std::vector<size_t> cols;
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, value)) {
                    cols.push_back(col);
                }
            }
            if (cols.size() == 2) {
                addEdge(cellIndex(row, cols[0]), cellIndex(row, cols[1]));
            }
        }

        // Columns
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            std::vector<size_t> rows;
            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, value)) {
                    rows.push_back(row);
                }
            }
            if (rows.size() == 2) {
                addEdge(cellIndex(rows[0], col), cellIndex(rows[1], col));
            }
        }

        // Boxes
        for (size_t box = 0; box < BOARD_SIZE; ++box) {
            size_t start_row = (box / BOX_SIZE) * BOX_SIZE;
            size_t start_col = (box % BOX_SIZE) * BOX_SIZE;
            std::vector<size_t> cells;
            for (size_t r = start_row; r < start_row + BOX_SIZE; ++r) {
                for (size_t c = start_col; c < start_col + BOX_SIZE; ++c) {
                    if (board[r][c] == EMPTY_CELL && candidates.isAllowed(r, c, value)) {
                        cells.push_back(cellIndex(r, c));
                    }
                }
            }
            if (cells.size() == 2) {
                addEdge(cells[0], cells[1]);
            }
        }

        return adj;
    }
};

}  // namespace sudoku::core
