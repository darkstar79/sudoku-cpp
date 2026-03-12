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

#include "../core/training_types.h"

#include <cstddef>
#include <optional>
#include <utility>

#include <QWidget>
#include <qcolor.h>
#include <qpoint.h>
#include <qtmetamacros.h>

namespace sudoku::view {

// CPD-OFF — board color constants shared with sudoku_board_widget
namespace TrainingBoardColors {
inline constexpr QColor BOARD_BORDER{44, 44, 44};
inline constexpr QColor BOARD_BACKGROUND{255, 255, 255};
inline constexpr QColor GRID_THICK_LINE{44, 44, 44};
inline constexpr QColor GRID_THIN_LINE{140, 140, 140};
inline constexpr QColor CELL_BACKGROUND{255, 255, 255};
inline constexpr QColor CELL_SELECTED{254, 243, 199};
inline constexpr QColor CELL_SELECTION_OUTLINE{20, 20, 20};
inline constexpr QColor TEXT_GIVEN{20, 20, 20};
inline constexpr QColor TEXT_CANDIDATE{107, 107, 107};
inline constexpr QColor TEXT_ELIMINATED{220, 38, 38};
inline constexpr QColor HIGHLIGHT_PATTERN{200, 220, 255};
inline constexpr QColor HIGHLIGHT_PIVOT{255, 220, 180};
inline constexpr QColor HIGHLIGHT_WING{220, 255, 220};
inline constexpr QColor HIGHLIGHT_FIN{255, 200, 200};
inline constexpr QColor HIGHLIGHT_CHAIN_A{180, 210, 255};
inline constexpr QColor HIGHLIGHT_CHAIN_B{180, 255, 200};
inline constexpr QColor HIGHLIGHT_SET_A{230, 200, 255};
inline constexpr QColor HIGHLIGHT_SET_B{255, 230, 200};
inline constexpr QColor HIGHLIGHT_SET_C{200, 255, 230};
inline constexpr QColor HIGHLIGHT_CORRECT{198, 246, 213};  // Soft green — correct answer
inline constexpr QColor HIGHLIGHT_WRONG{254, 215, 215};    // Soft red — wrong answer
inline constexpr QColor HIGHLIGHT_MISSED{254, 252, 191};   // Soft yellow — missed answer
inline constexpr QColor HOVER_TINT{200, 220, 255, 80};     // Subtle blue overlay for hover
inline constexpr QColor COLOR_A{100, 149, 237};            // Cornflower blue
inline constexpr QColor COLOR_B{60, 179, 113};             // Medium sea green
}  // namespace TrainingBoardColors

/// Interactive Sudoku board widget for training exercises.
/// Renders a TrainingBoard and supports cell selection.
class TrainingBoardWidget : public QWidget {
    Q_OBJECT

public:
    explicit TrainingBoardWidget(QWidget* parent = nullptr);

    /// Update the board data and repaint
    void setBoard(const core::TrainingBoard& board);

    /// Get the current board state (with player modifications)
    [[nodiscard]] const core::TrainingBoard& board() const {
        return board_;
    }

    /// Get currently selected cell position
    [[nodiscard]] std::optional<std::pair<size_t, size_t>> selectedCell() const {
        return selected_cell_;
    }

    /// Set read-only mode (disables interaction, used on feedback page)
    void setReadOnly(bool read_only);

    /// Apply a number action to the selected cell (placement or elimination depending on mode)
    void applyNumber(int value, core::TrainingInteractionMode mode);

    /// Apply a color to the selected cell
    void applyColor(int color);

    // CPD-OFF — Qt widget interface boilerplate
    [[nodiscard]] QSize minimumSizeHint() const override;
    [[nodiscard]] QSize sizeHint() const override;

signals:
    /// Emitted when the player modifies the board
    void boardChanged(const core::TrainingBoard& board);

    /// Emitted when a cell is selected
    void cellSelected(size_t row, size_t col);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    core::TrainingBoard board_{};
    std::optional<std::pair<size_t, size_t>> selected_cell_;
    bool read_only_{false};
    int hovered_candidate_{0};  ///< Currently hovered candidate value (0 = none)

    [[nodiscard]] float cellSize() const;
    [[nodiscard]] QPointF boardOrigin() const;

    /// Determine which candidate (1-9) is under the given cell-relative position
    [[nodiscard]] static int candidateAtPosition(float local_x, float local_y, float cell_size);

    void paintBackground(QPainter& painter, const QPointF& origin, float board_size);
    void paintCell(QPainter& painter, size_t row, size_t col, const QPointF& origin, float cell_size);
    void paintCellValue(QPainter& painter, const core::TrainingCellState& cell, const QRectF& cell_rect);
    void paintCellCandidates(QPainter& painter, const core::TrainingCellState& cell, const QRectF& cell_rect);
    void paintGridLines(QPainter& painter, const QPointF& origin, float board_size, float cell_size);

    // CPD-ON
    [[nodiscard]] static QColor cellRoleColor(core::CellRole role);
    [[nodiscard]] static QColor playerColorBackground(int player_color);
};

}  // namespace sudoku::view
