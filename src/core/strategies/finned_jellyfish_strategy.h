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

#include <algorithm>
#include <optional>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Finned Jellyfish patterns.
/// Like a Jellyfish (4x4 fish), but one of the 4 rows (or columns) has an extra candidate (the "fin").
/// The union of columns is 5 instead of 4. Eliminations are restricted to the 4 base columns
/// in cells that also share the fin's box.
class FinnedJellyfishStrategy : public ISolvingStrategy, protected StrategyBase {
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
        return SolvingTechnique::FinnedJellyfish;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Finned Jellyfish";
    }

    [[nodiscard]] int getDifficultyPoints() const override {
        return getTechniquePoints(SolvingTechnique::FinnedJellyfish);
    }

private:
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — value×row1×row2×row3×row4 search; nesting is inherent to fish algorithms
    [[nodiscard]] static std::optional<SolveStep> findRowBased(const std::vector<std::vector<int>>& board,
                                                               const CandidateGrid& candidates) {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            std::vector<std::vector<size_t>> rows_cols(BOARD_SIZE);
            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                for (size_t col = 0; col < BOARD_SIZE; ++col) {
                    if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, value)) {
                        rows_cols[row].push_back(col);
                    }
                }
            }

            for (size_t r1 = 0; r1 < BOARD_SIZE; ++r1) {
                if (rows_cols[r1].size() < 2 || rows_cols[r1].size() > 5) {
                    continue;
                }
                for (size_t r2 = r1 + 1; r2 < BOARD_SIZE; ++r2) {
                    if (rows_cols[r2].size() < 2 || rows_cols[r2].size() > 5) {
                        continue;
                    }
                    for (size_t r3 = r2 + 1; r3 < BOARD_SIZE; ++r3) {
                        if (rows_cols[r3].size() < 2 || rows_cols[r3].size() > 5) {
                            continue;
                        }
                        for (size_t r4 = r3 + 1; r4 < BOARD_SIZE; ++r4) {
                            if (rows_cols[r4].size() < 2 || rows_cols[r4].size() > 5) {
                                continue;
                            }

                            auto union_cols = indexUnion(rows_cols[r1], rows_cols[r2], rows_cols[r3], rows_cols[r4]);
                            if (union_cols.size() != 5) {
                                continue;
                            }

                            auto result = tryRowFinned(board, candidates, value, rows_cols, r1, r2, r3, r4, union_cols);
                            if (result.has_value()) {
                                return result;
                            }
                        }
                    }
                }
            }
        }
        return std::nullopt;
    }

    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — fin candidate enumeration over 4-row Jellyfish; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> tryRowFinned(const std::vector<std::vector<int>>& board,
                                                               const CandidateGrid& candidates, int value,
                                                               const std::vector<std::vector<size_t>>& rows_cols,
                                                               size_t r1, size_t r2, size_t r3, size_t r4,
                                                               const std::vector<size_t>& union_cols) {
        std::array<size_t, 4> rows = {r1, r2, r3, r4};

        // Try each of the 5 cols as the fin col
        for (size_t fi = 0; fi < 5; ++fi) {
            size_t fin_col = union_cols[fi];
            std::vector<size_t> base_cols;
            for (size_t ci = 0; ci < 5; ++ci) {
                if (ci != fi) {
                    base_cols.push_back(union_cols[ci]);
                }
            }

            // Exactly one row should have the fin col
            size_t fin_row = 0;
            int fin_row_count = 0;
            for (size_t row : rows) {
                bool has_fin = std::ranges::contains(rows_cols[row], fin_col);
                if (has_fin) {
                    fin_row = row;
                    ++fin_row_count;
                }
            }

            if (fin_row_count != 1) {
                continue;
            }

            // Non-finned rows' candidates must all be within base_cols
            bool valid = true;
            for (size_t row : rows) {
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

            std::vector<Elimination> eliminations;
            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                if (row == r1 || row == r2 || row == r3 || row == r4) {
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
            for (size_t row : rows) {
                for (size_t col : rows_cols[row]) {
                    positions.push_back(Position{.row = row, .col = col});
                }
            }

            auto explanation = fmt::format("Finned Jellyfish on value {} in Rows {}, {}, {}, {} with fin at {} — "
                                           "eliminates {} from cells in fin's box",
                                           value, r1 + 1, r2 + 1, r3 + 1, r4 + 1, formatPosition(fin_pos), value);

            std::vector<CellRole> roles;
            roles.reserve(positions.size());
            for (const auto& pos : positions) {
                roles.push_back(pos == fin_pos ? CellRole::Fin : CellRole::Pattern);
            }

            return SolveStep{.type = SolveStepType::Elimination,
                             .technique = SolvingTechnique::FinnedJellyfish,
                             .position = eliminations[0].position,
                             .value = 0,
                             .eliminations = eliminations,
                             .explanation = explanation,
                             .points = getTechniquePoints(SolvingTechnique::FinnedJellyfish),
                             .explanation_data = {.positions = positions,
                                                  .values = {value, static_cast<int>(r1 + 1), static_cast<int>(r2 + 1),
                                                             static_cast<int>(r3 + 1), static_cast<int>(r4 + 1)},
                                                  .region_type = RegionType::Row,
                                                  .position_roles = std::move(roles)}};
        }
        return std::nullopt;
    }

    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — value×col1×col2×col3×col4 search; nesting is inherent to fish algorithms
    [[nodiscard]] static std::optional<SolveStep> findColBased(const std::vector<std::vector<int>>& board,
                                                               const CandidateGrid& candidates) {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            std::vector<std::vector<size_t>> cols_rows(BOARD_SIZE);
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                for (size_t row = 0; row < BOARD_SIZE; ++row) {
                    if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, value)) {
                        cols_rows[col].push_back(row);
                    }
                }
            }

            for (size_t c1 = 0; c1 < BOARD_SIZE; ++c1) {
                if (cols_rows[c1].size() < 2 || cols_rows[c1].size() > 5) {
                    continue;
                }
                for (size_t c2 = c1 + 1; c2 < BOARD_SIZE; ++c2) {
                    if (cols_rows[c2].size() < 2 || cols_rows[c2].size() > 5) {
                        continue;
                    }
                    for (size_t c3 = c2 + 1; c3 < BOARD_SIZE; ++c3) {
                        if (cols_rows[c3].size() < 2 || cols_rows[c3].size() > 5) {
                            continue;
                        }
                        for (size_t c4 = c3 + 1; c4 < BOARD_SIZE; ++c4) {
                            if (cols_rows[c4].size() < 2 || cols_rows[c4].size() > 5) {
                                continue;
                            }

                            auto union_rows = indexUnion(cols_rows[c1], cols_rows[c2], cols_rows[c3], cols_rows[c4]);
                            if (union_rows.size() != 5) {
                                continue;
                            }

                            auto result = tryColFinned(board, candidates, value, cols_rows, c1, c2, c3, c4, union_rows);
                            if (result.has_value()) {
                                return result;
                            }
                        }
                    }
                }
            }
        }
        return std::nullopt;
    }

    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — fin candidate enumeration over 4-col Jellyfish; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> tryColFinned(const std::vector<std::vector<int>>& board,
                                                               const CandidateGrid& candidates, int value,
                                                               const std::vector<std::vector<size_t>>& cols_rows,
                                                               size_t c1, size_t c2, size_t c3, size_t c4,
                                                               const std::vector<size_t>& union_rows) {
        std::array<size_t, 4> cols = {c1, c2, c3, c4};

        for (size_t fi = 0; fi < 5; ++fi) {
            size_t fin_row = union_rows[fi];
            std::vector<size_t> base_rows;
            for (size_t ri = 0; ri < 5; ++ri) {
                if (ri != fi) {
                    base_rows.push_back(union_rows[ri]);
                }
            }

            size_t fin_col = 0;
            int fin_col_count = 0;
            for (size_t col : cols) {
                bool has_fin = std::ranges::contains(cols_rows[col], fin_row);
                if (has_fin) {
                    fin_col = col;
                    ++fin_col_count;
                }
            }

            if (fin_col_count != 1) {
                continue;
            }

            bool valid = true;
            for (size_t col : cols) {
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
                if (col == c1 || col == c2 || col == c3 || col == c4) {
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
            for (size_t col : cols) {
                for (size_t row : cols_rows[col]) {
                    positions.push_back(Position{.row = row, .col = col});
                }
            }

            auto explanation = fmt::format("Finned Jellyfish on value {} in Columns {}, {}, {}, {} with fin at {} — "
                                           "eliminates {} from cells in fin's box",
                                           value, c1 + 1, c2 + 1, c3 + 1, c4 + 1, formatPosition(fin_pos), value);

            std::vector<CellRole> roles;
            roles.reserve(positions.size());
            for (const auto& pos : positions) {
                roles.push_back(pos == fin_pos ? CellRole::Fin : CellRole::Pattern);
            }

            return SolveStep{.type = SolveStepType::Elimination,
                             .technique = SolvingTechnique::FinnedJellyfish,
                             .position = eliminations[0].position,
                             .value = 0,
                             .eliminations = eliminations,
                             .explanation = explanation,
                             .points = getTechniquePoints(SolvingTechnique::FinnedJellyfish),
                             .explanation_data = {.positions = positions,
                                                  .values = {value, static_cast<int>(c1 + 1), static_cast<int>(c2 + 1),
                                                             static_cast<int>(c3 + 1), static_cast<int>(c4 + 1)},
                                                  .region_type = RegionType::Col,
                                                  .position_roles = std::move(roles)}};
        }
        return std::nullopt;
    }

    /// Sorted union of 4 index vectors.
    [[nodiscard]] static std::vector<size_t> indexUnion(const std::vector<size_t>& first,
                                                        const std::vector<size_t>& second,
                                                        const std::vector<size_t>& third,
                                                        const std::vector<size_t>& fourth) {
        std::vector<size_t> result;
        result.reserve(12);
        auto add = [&](size_t idx) {
            if (!std::ranges::contains(result, idx)) {
                result.push_back(idx);
            }
        };
        for (size_t idx : first) {
            add(idx);
        }
        for (size_t idx : second) {
            add(idx);
        }
        for (size_t idx : third) {
            add(idx);
        }
        for (size_t idx : fourth) {
            add(idx);
        }
        std::ranges::sort(result);
        return result;
    }
};

}  // namespace sudoku::core
