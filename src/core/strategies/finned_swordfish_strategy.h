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

#include "../i_solving_strategy.h"
#include "../solve_step.h"
#include "../solving_technique.h"
#include "../strategy_base.h"
#include "fish_helpers.h"

#include <optional>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Finned Swordfish patterns.
/// Like a Swordfish, but one of the 3 rows (or columns) has an extra candidate (the "fin").
/// The union of columns is 4 instead of 3. Eliminations are restricted to the 3 base columns
/// in cells that also share the fin's box.
class FinnedSwordfishStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const std::vector<std::vector<int>>& board,
                                                    const CandidateGrid& candidates) const override {
        auto result = findRowBased(board, candidates);
        if (result.has_value()) {
            return result;
        }
        return findColBased(board, candidates);
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::FinnedSwordfish;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Finned Swordfish";
    }

    [[nodiscard]] int getDifficultyPoints() const override {
        return getTechniquePoints(SolvingTechnique::FinnedSwordfish);
    }

private:
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — value×row1×row2×row3 search; nesting is inherent to fish algorithms
    [[nodiscard]] static std::optional<SolveStep> findRowBased(const std::vector<std::vector<int>>& board,
                                                               const CandidateGrid& candidates) {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            auto rows_cols = FishHelpers::collectCandidatePositions(board, candidates, value, true);

            for (size_t r1 = 0; r1 < BOARD_SIZE; ++r1) {
                if (rows_cols[r1].size() < 2 || rows_cols[r1].size() > 4) {
                    continue;
                }
                for (size_t r2 = r1 + 1; r2 < BOARD_SIZE; ++r2) {
                    if (rows_cols[r2].size() < 2 || rows_cols[r2].size() > 4) {
                        continue;
                    }
                    for (size_t r3 = r2 + 1; r3 < BOARD_SIZE; ++r3) {
                        if (rows_cols[r3].size() < 2 || rows_cols[r3].size() > 4) {
                            continue;
                        }

                        auto union_cols = FishHelpers::indexUnion(rows_cols[r1], rows_cols[r2], rows_cols[r3]);
                        if (union_cols.size() != 4) {
                            continue;
                        }

                        // Try each combination of 3 base cols from the 4-col union
                        auto result =
                            tryRowFinnedSwordfish(board, candidates, value, rows_cols, r1, r2, r3, union_cols);
                        if (result.has_value()) {
                            return result;
                        }
                    }
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] static std::optional<SolveStep>
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — fin candidate enumeration over 3-row Swordfish; nesting is inherent
    tryRowFinnedSwordfish(const std::vector<std::vector<int>>& board, const CandidateGrid& candidates, int value,
                          const std::vector<std::vector<size_t>>& rows_cols, size_t r1, size_t r2, size_t r3,
                          const std::vector<size_t>& union_cols) {
        // Try each of the 4 cols as the fin col
        for (size_t fi = 0; fi < 4; ++fi) {
            size_t fin_col = union_cols[fi];
            std::vector<size_t> base_cols;
            for (size_t ci = 0; ci < 4; ++ci) {
                if (ci != fi) {
                    base_cols.push_back(union_cols[ci]);
                }
            }

            // Identify the finned row: the row that has the fin_col
            // Exactly one row should have the fin col AND all its other positions should be in base_cols
            size_t fin_row = 0;
            int fin_row_count = 0;
            for (size_t row : {r1, r2, r3}) {
                bool has_fin = false;
                for (size_t col : rows_cols[row]) {
                    if (col == fin_col) {
                        has_fin = true;
                        break;
                    }
                }
                if (has_fin) {
                    fin_row = row;
                    ++fin_row_count;
                }
            }

            // For a valid finned swordfish, exactly one row should contain the fin col
            if (fin_row_count != 1) {
                continue;
            }

            // Verify each non-finned row's candidates are within base_cols
            bool valid = true;
            for (size_t row : {r1, r2, r3}) {
                if (row == fin_row) {
                    continue;
                }
                for (size_t col : rows_cols[row]) {
                    if (!std::ranges::contains(base_cols, col)) {
                        valid = false;
                        break;
                    }
                }
                if (!valid) {
                    break;
                }
            }
            if (!valid) {
                continue;
            }

            Position fin_pos{.row = fin_row, .col = fin_col};
            size_t fin_box = getBoxIndex(fin_row, fin_col);

            // Eliminate value from base cols in non-pattern rows, but only in fin's box
            std::vector<Elimination> eliminations;
            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                if (row == r1 || row == r2 || row == r3) {
                    continue;
                }
                for (size_t col : base_cols) {
                    if (getBoxIndex(row, col) == fin_box && board[row][col] == EMPTY_CELL &&
                        candidates.isAllowed(row, col, value)) {
                        eliminations.push_back(
                            Elimination{.position = Position{.row = row, .col = col}, .value = value});
                    }
                }
            }

            if (eliminations.empty()) {
                continue;
            }

            std::vector<Position> positions;
            for (size_t row : {r1, r2, r3}) {
                for (size_t col : rows_cols[row]) {
                    positions.push_back(Position{.row = row, .col = col});
                }
            }

            auto explanation = fmt::format("Finned Swordfish on value {} in Rows {}, {}, {} with fin at {} — "
                                           "eliminates {} from cells in fin's box",
                                           value, r1 + 1, r2 + 1, r3 + 1, formatPosition(fin_pos), value);

            std::vector<CellRole> roles;
            roles.reserve(positions.size());
            for (const auto& pos : positions) {
                roles.push_back(pos == fin_pos ? CellRole::Fin : CellRole::Pattern);
            }

            return SolveStep{.type = SolveStepType::Elimination,
                             .technique = SolvingTechnique::FinnedSwordfish,
                             .position = eliminations[0].position,
                             .value = 0,
                             .eliminations = eliminations,
                             .explanation = explanation,
                             .points = getTechniquePoints(SolvingTechnique::FinnedSwordfish),
                             .explanation_data = {.positions = positions,
                                                  .values = {value, static_cast<int>(r1 + 1), static_cast<int>(r2 + 1),
                                                             static_cast<int>(r3 + 1)},
                                                  .region_type = RegionType::Row,
                                                  .position_roles = std::move(roles)}};
        }
        return std::nullopt;
    }

    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — value×col1×col2×col3 search; nesting is inherent to fish algorithms
    [[nodiscard]] static std::optional<SolveStep> findColBased(const std::vector<std::vector<int>>& board,
                                                               const CandidateGrid& candidates) {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            auto cols_rows = FishHelpers::collectCandidatePositions(board, candidates, value, false);

            for (size_t c1 = 0; c1 < BOARD_SIZE; ++c1) {
                if (cols_rows[c1].size() < 2 || cols_rows[c1].size() > 4) {
                    continue;
                }
                for (size_t c2 = c1 + 1; c2 < BOARD_SIZE; ++c2) {
                    if (cols_rows[c2].size() < 2 || cols_rows[c2].size() > 4) {
                        continue;
                    }
                    for (size_t c3 = c2 + 1; c3 < BOARD_SIZE; ++c3) {
                        if (cols_rows[c3].size() < 2 || cols_rows[c3].size() > 4) {
                            continue;
                        }

                        auto union_rows = FishHelpers::indexUnion(cols_rows[c1], cols_rows[c2], cols_rows[c3]);
                        if (union_rows.size() != 4) {
                            continue;
                        }

                        auto result =
                            tryColFinnedSwordfish(board, candidates, value, cols_rows, c1, c2, c3, union_rows);
                        if (result.has_value()) {
                            return result;
                        }
                    }
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] static std::optional<SolveStep>
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — fin candidate enumeration over 3-col Swordfish; nesting is inherent
    tryColFinnedSwordfish(const std::vector<std::vector<int>>& board, const CandidateGrid& candidates, int value,
                          const std::vector<std::vector<size_t>>& cols_rows, size_t c1, size_t c2, size_t c3,
                          const std::vector<size_t>& union_rows) {
        for (size_t fi = 0; fi < 4; ++fi) {
            size_t fin_row = union_rows[fi];
            std::vector<size_t> base_rows;
            for (size_t ri = 0; ri < 4; ++ri) {
                if (ri != fi) {
                    base_rows.push_back(union_rows[ri]);
                }
            }

            size_t fin_col = 0;
            int fin_col_count = 0;
            for (size_t col : {c1, c2, c3}) {
                bool has_fin = false;
                for (size_t row : cols_rows[col]) {
                    if (row == fin_row) {
                        has_fin = true;
                        break;
                    }
                }
                if (has_fin) {
                    fin_col = col;
                    ++fin_col_count;
                }
            }

            if (fin_col_count != 1) {
                continue;
            }

            bool valid = true;
            for (size_t col : {c1, c2, c3}) {
                if (col == fin_col) {
                    continue;
                }
                for (size_t row : cols_rows[col]) {
                    if (!std::ranges::contains(base_rows, row)) {
                        valid = false;
                        break;
                    }
                }
                if (!valid) {
                    break;
                }
            }
            if (!valid) {
                continue;
            }

            Position fin_pos{.row = fin_row, .col = fin_col};
            size_t fin_box = getBoxIndex(fin_row, fin_col);

            std::vector<Elimination> eliminations;
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (col == c1 || col == c2 || col == c3) {
                    continue;
                }
                for (size_t row : base_rows) {
                    if (getBoxIndex(row, col) == fin_box && board[row][col] == EMPTY_CELL &&
                        candidates.isAllowed(row, col, value)) {
                        eliminations.push_back(
                            Elimination{.position = Position{.row = row, .col = col}, .value = value});
                    }
                }
            }

            if (eliminations.empty()) {
                continue;
            }

            std::vector<Position> positions;
            for (size_t col : {c1, c2, c3}) {
                for (size_t row : cols_rows[col]) {
                    positions.push_back(Position{.row = row, .col = col});
                }
            }

            auto explanation = fmt::format("Finned Swordfish on value {} in Columns {}, {}, {} with fin at {} — "
                                           "eliminates {} from cells in fin's box",
                                           value, c1 + 1, c2 + 1, c3 + 1, formatPosition(fin_pos), value);

            std::vector<CellRole> roles;
            roles.reserve(positions.size());
            for (const auto& pos : positions) {
                roles.push_back(pos == fin_pos ? CellRole::Fin : CellRole::Pattern);
            }

            return SolveStep{.type = SolveStepType::Elimination,
                             .technique = SolvingTechnique::FinnedSwordfish,
                             .position = eliminations[0].position,
                             .value = 0,
                             .eliminations = eliminations,
                             .explanation = explanation,
                             .points = getTechniquePoints(SolvingTechnique::FinnedSwordfish),
                             .explanation_data = {.positions = positions,
                                                  .values = {value, static_cast<int>(c1 + 1), static_cast<int>(c2 + 1),
                                                             static_cast<int>(c3 + 1)},
                                                  .region_type = RegionType::Col,
                                                  .position_roles = std::move(roles)}};
        }
        return std::nullopt;
    }
};

}  // namespace sudoku::core
