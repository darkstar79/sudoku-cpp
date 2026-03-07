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
#include "../view_model/training_view_model.h"

#include <memory>

#include <QWidget>

class QStackedWidget;
class QVBoxLayout;

namespace sudoku::view {

class TrainingBoardWidget;

class TrainingWidget : public QWidget {
    Q_OBJECT

public:
    explicit TrainingWidget(QWidget* parent = nullptr);

    void setTrainingViewModel(std::shared_ptr<viewmodel::TrainingViewModel> training_vm);

signals:
    void backToGame();

private:
    std::shared_ptr<viewmodel::TrainingViewModel> training_vm_;
    core::CompositeObserver observer_;
    QStackedWidget* pages_{nullptr};

    void buildTechniqueSelectionPage();
    void buildTheoryPage();
    void buildExercisePage();
    void buildFeedbackPage();
    void buildLessonCompletePage();

    void refreshCurrentPage();
};

}  // namespace sudoku::view
