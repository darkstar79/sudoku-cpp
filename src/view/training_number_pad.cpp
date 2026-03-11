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

#include "training_number_pad.h"

#include "core/constants.h"
#include "core/training_types.h"

#include <algorithm>

#include <QHBoxLayout>
#include <QPushButton>
#include <qstring.h>

namespace sudoku::view {

TrainingNumberPad::TrainingNumberPad(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    for (int i = 0; i < static_cast<int>(core::MAX_VALUE); ++i) {
        int value = i + 1;
        auto* btn = new QPushButton(QString::number(value));
        btn->setFixedSize(44, 44);
        btn->setStyleSheet("font-size: 16px; font-weight: bold;");

        connect(btn, &QPushButton::clicked, this, [this, value]() { emit numberPressed(value); });

        buttons_[static_cast<size_t>(i)] = btn;
        layout->addWidget(btn);
    }

    layout->addStretch();
}

void TrainingNumberPad::setInteractionMode(core::TrainingInteractionMode mode) {
    mode_ = mode;

    for (int i = 0; i < static_cast<int>(core::MAX_VALUE); ++i) {
        if (mode == core::TrainingInteractionMode::Placement) {
            buttons_[static_cast<size_t>(i)]->setToolTip(QString("Place %1 in selected cell").arg(i + 1));
        } else {
            buttons_[static_cast<size_t>(i)]->setToolTip(QString("Eliminate %1 from selected cell").arg(i + 1));
        }
    }
}

void TrainingNumberPad::updateEnabledNumbers(const std::vector<int>& candidates) {
    for (int i = 0; i < static_cast<int>(core::MAX_VALUE); ++i) {
        int value = i + 1;
        bool enabled = std::ranges::find(candidates, value) != candidates.end();
        buttons_[static_cast<size_t>(i)]->setEnabled(enabled);
    }
}

}  // namespace sudoku::view
