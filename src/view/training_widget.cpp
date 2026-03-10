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

#include "training_widget.h"

#include "../core/solving_technique.h"
#include "core/technique_descriptions.h"
#include "core/training_types.h"
#include "view_model/training_view_model.h"

#include <array>
#include <iterator>
#include <string_view>
#include <utility>
#include <vector>

#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QtCore/qobjectdefs.h>
#include <fmt/format.h>
#include <qnamespace.h>
#include <qstring.h>
#include <qstringalgorithms.h>

namespace sudoku::view {

TrainingWidget::TrainingWidget(QWidget* parent) : QWidget(parent), pages_(new QStackedWidget) {
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(pages_);

    // Create 5 pages for each training phase
    buildTechniqueSelectionPage();
    buildTheoryPage();
    buildExercisePage();
    buildFeedbackPage();
    buildLessonCompletePage();
}

void TrainingWidget::setTrainingViewModel(std::shared_ptr<viewmodel::TrainingViewModel> training_vm) {
    training_vm_ = std::move(training_vm);
    if (training_vm_) {
        observer_.observe(training_vm_->trainingState, [this](const auto&) { refreshCurrentPage(); });
    }
}

void TrainingWidget::refreshCurrentPage() {
    if (!training_vm_) {
        return;
    }

    const auto& state = training_vm_->trainingState.get();

    switch (state.phase) {
        case core::TrainingPhase::TechniqueSelection:
            pages_->setCurrentIndex(0);
            break;
        case core::TrainingPhase::TheoryReview:
            pages_->setCurrentIndex(1);
            break;
        case core::TrainingPhase::Exercise:
            pages_->setCurrentIndex(2);
            break;
        case core::TrainingPhase::Feedback:
            pages_->setCurrentIndex(3);
            break;
        case core::TrainingPhase::LessonComplete:
            pages_->setCurrentIndex(4);
            break;
    }

    // Update content of the current page dynamically
    // Each page reads from training_vm_ when shown
    pages_->currentWidget()->update();
}

void TrainingWidget::buildTechniqueSelectionPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* title = new QLabel("<h1>Training Mode</h1>");
    layout->addWidget(title);
    layout->addWidget(new QLabel("Select a technique to practice:"));

    // clang-format off
    struct TechniqueGroup {
        const char* name;
        std::vector<core::SolvingTechnique> techniques;
    };

    std::array<TechniqueGroup, 9> groups = {{
        {.name = "Foundations", .techniques = {core::SolvingTechnique::NakedSingle, core::SolvingTechnique::HiddenSingle}},
        {.name = "Subset Basics", .techniques = {core::SolvingTechnique::NakedPair, core::SolvingTechnique::NakedTriple,
                           core::SolvingTechnique::HiddenPair, core::SolvingTechnique::HiddenTriple}},
        {.name = "Intersections & Quads", .techniques = {core::SolvingTechnique::PointingPair, core::SolvingTechnique::BoxLineReduction,
                                    core::SolvingTechnique::NakedQuad, core::SolvingTechnique::HiddenQuad}},
        {.name = "Basic Fish & Wings", .techniques = {core::SolvingTechnique::XWing, core::SolvingTechnique::XYWing,
                                 core::SolvingTechnique::Swordfish, core::SolvingTechnique::Skyscraper,
                                 core::SolvingTechnique::TwoStringKite, core::SolvingTechnique::XYZWing}},
        {.name = "Links & Rectangles", .techniques = {core::SolvingTechnique::UniqueRectangle, core::SolvingTechnique::WWing,
                                 core::SolvingTechnique::SimpleColoring, core::SolvingTechnique::FinnedXWing,
                                 core::SolvingTechnique::RemotePairs, core::SolvingTechnique::BUG}},
        {.name = "Advanced Fish & Wings", .techniques = {core::SolvingTechnique::Jellyfish, core::SolvingTechnique::FinnedSwordfish,
                                    core::SolvingTechnique::EmptyRectangle, core::SolvingTechnique::WXYZWing,
                                    core::SolvingTechnique::MultiColoring}},
        {.name = "Advanced Fish (Finned)", .techniques = {core::SolvingTechnique::FinnedJellyfish}},
        {.name = "Chains & Set Logic", .techniques = {core::SolvingTechnique::XYChain, core::SolvingTechnique::ALSxZ,
                                 core::SolvingTechnique::SueDeCoq}},
        {.name = "Inference Engines", .techniques = {core::SolvingTechnique::ForcingChain, core::SolvingTechnique::NiceLoop}},
    }};
    // clang-format on

    for (const auto& group : groups) {
        auto* group_box = new QGroupBox(group.name);
        auto* group_layout = new QVBoxLayout(
            group_box);  // NOLINT(cppcoreguidelines-owning-memory,clang-analyzer-cplusplus.NewDeleteLeaks) — Qt parent takes ownership

        for (auto technique : group.techniques) {
            auto name = core::getTechniqueName(technique);
            auto points = core::getTechniquePoints(technique);
            auto label = fmt::format("{} ({} pts)", name, points);

            auto* btn = new QPushButton(QString::fromStdString(label));
            btn->setFlat(true);
            btn->setStyleSheet("text-align: left; padding: 4px 8px;");
            connect(btn, &QPushButton::clicked, this, [this, technique]() {
                if (training_vm_) {
                    training_vm_->selectTechnique(technique);
                }
            });
            group_layout->addWidget(btn);
        }

        layout->addWidget(
            group_box);  // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks) — group_layout owned by group_box
    }

    auto* back_btn = new QPushButton("Back to Game");
    connect(back_btn, &QPushButton::clicked, this, &TrainingWidget::backToGame);
    layout->addWidget(back_btn);

    layout->addStretch();

    // Wrap in scroll area
    auto* scroll = new QScrollArea;
    scroll->setWidget(page);
    scroll->setWidgetResizable(true);
    pages_->addWidget(scroll);
}

void TrainingWidget::buildTheoryPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* title_label = new QLabel;
    title_label->setObjectName("theoryTitle");
    title_label->setStyleSheet("font-size: 24px; font-weight: bold;");
    layout->addWidget(title_label);

    auto* points_label = new QLabel;
    points_label->setObjectName("theoryPoints");
    points_label->setStyleSheet("color: #666;");
    layout->addWidget(points_label);

    layout->addWidget(new QLabel("<b>What It Is:</b>"));
    auto* what_label = new QLabel;
    what_label->setObjectName("theoryWhat");
    what_label->setWordWrap(true);
    layout->addWidget(what_label);

    layout->addWidget(new QLabel("<b>What to Look For:</b>"));
    auto* look_label = new QLabel;
    look_label->setObjectName("theoryLook");
    look_label->setWordWrap(true);
    layout->addWidget(look_label);

    auto* btn_layout = new QHBoxLayout;
    auto* start_btn = new QPushButton("Start Exercises");
    auto* back_btn = new QPushButton("Back");
    btn_layout->addWidget(start_btn);
    btn_layout->addWidget(back_btn);
    btn_layout->addStretch();
    layout->addLayout(btn_layout);
    layout->addStretch();

    connect(start_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->startExercises();
        }
    });
    connect(back_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->returnToSelection();
        }
    });

    // Dynamic content update via showEvent-like mechanism
    connect(pages_, &QStackedWidget::currentChanged, this,
            [this, title_label, points_label, what_label, look_label](int index) {
                if (index != 1 || !training_vm_) {
                    return;
                }
                auto desc = training_vm_->currentDescription();
                const auto& state = training_vm_->trainingState.get();
                auto points = core::getTechniquePoints(state.current_technique);

                title_label->setText(QString::fromUtf8(desc.title.data(), static_cast<int>(desc.title.size())));
                points_label->setText(QString("%1 difficulty points").arg(points));
                what_label->setText(
                    QString::fromUtf8(desc.what_it_is.data(), static_cast<int>(desc.what_it_is.size())));
                look_label->setText(
                    QString::fromUtf8(desc.what_to_look_for.data(), static_cast<int>(desc.what_to_look_for.size())));
            });

    pages_->addWidget(page);
}

void TrainingWidget::buildExercisePage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* header = new QLabel;
    header->setObjectName("exerciseHeader");
    layout->addWidget(header);

    // Placeholder for training board (text rendering for now)
    auto* board_placeholder = new QLabel("[ Training Board ]");
    board_placeholder->setObjectName("exerciseBoard");
    board_placeholder->setAlignment(Qt::AlignCenter);
    board_placeholder->setMinimumSize(400, 400);
    board_placeholder->setStyleSheet("background-color: white; border: 2px solid #2c2c2c;");
    layout->addWidget(board_placeholder, 1);

    auto* btn_layout = new QHBoxLayout;
    auto* submit_btn = new QPushButton("Submit");
    auto* hint_btn = new QPushButton("Hint");
    auto* skip_btn = new QPushButton("Skip");
    auto* quit_btn = new QPushButton("Quit Lesson");
    btn_layout->addWidget(submit_btn);
    btn_layout->addWidget(hint_btn);
    btn_layout->addWidget(skip_btn);
    btn_layout->addStretch();
    btn_layout->addWidget(quit_btn);
    layout->addLayout(btn_layout);

    connect(submit_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->submitAnswer(training_vm_->trainingBoard.get());
        }
    });
    connect(hint_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->requestHint();
        }
    });
    connect(skip_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->skipExercise();
        }
    });
    connect(quit_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->returnToSelection();
        }
    });

    connect(pages_, &QStackedWidget::currentChanged, this, [this, header](int index) {
        if (index != 2 || !training_vm_) {
            return;
        }
        const auto& state = training_vm_->trainingState.get();
        auto name = core::getTechniqueName(state.current_technique);
        header->setText(QString("Exercise %1 / %2  -  %3")
                            .arg(state.exercise_index + 1)
                            .arg(state.total_exercises)
                            .arg(QString::fromUtf8(name.data(), static_cast<int>(name.size()))));
    });

    pages_->addWidget(page);
}

void TrainingWidget::buildFeedbackPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* result_label = new QLabel;
    result_label->setObjectName("feedbackResult");
    result_label->setStyleSheet("font-size: 24px; font-weight: bold;");
    layout->addWidget(result_label);

    auto* msg_label = new QLabel;
    msg_label->setObjectName("feedbackMessage");
    msg_label->setWordWrap(true);
    layout->addWidget(msg_label);

    auto* score_label = new QLabel;
    score_label->setObjectName("feedbackScore");
    layout->addWidget(score_label);

    auto* btn_layout = new QHBoxLayout;
    auto* next_btn = new QPushButton("Next Exercise");
    auto* retry_btn = new QPushButton("Retry");
    auto* quit_btn = new QPushButton("Quit Lesson");
    btn_layout->addWidget(next_btn);
    btn_layout->addWidget(retry_btn);
    btn_layout->addStretch();
    btn_layout->addWidget(quit_btn);
    layout->addLayout(btn_layout);
    layout->addStretch();

    connect(next_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->nextExercise();
        }
    });
    connect(retry_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->retryExercise();
        }
    });
    connect(quit_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->returnToSelection();
        }
    });

    connect(pages_, &QStackedWidget::currentChanged, this, [this, result_label, msg_label, score_label](int index) {
        if (index != 3 || !training_vm_) {
            return;
        }
        const auto& state = training_vm_->trainingState.get();

        QString result_text;
        QString color;
        switch (state.last_result) {
            case core::AnswerResult::Correct:
                result_text = "Correct!";
                color = "color: green;";
                break;
            case core::AnswerResult::PartiallyCorrect:
                result_text = "Partially Correct";
                color = "color: #cc9900;";
                break;
            case core::AnswerResult::Incorrect:
                result_text = "Incorrect";
                color = "color: red;";
                break;
        }

        result_label->setText(result_text);
        result_label->setStyleSheet(QString("font-size: 24px; font-weight: bold; %1").arg(color));
        msg_label->setText(QString::fromStdString(state.feedback_message));
        score_label->setText(QString("Score: %1 / %2").arg(state.correct_count).arg(state.exercise_index + 1));
    });

    pages_->addWidget(page);
}

void TrainingWidget::buildLessonCompletePage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* title = new QLabel("<h1>Lesson Complete!</h1>");
    layout->addWidget(title);

    auto* info_label = new QLabel;
    info_label->setObjectName("completeInfo");
    layout->addWidget(info_label);

    auto* verdict_label = new QLabel;
    verdict_label->setObjectName("completeVerdict");
    layout->addWidget(verdict_label);

    auto* btn_layout = new QHBoxLayout;
    auto* again_btn = new QPushButton("Try Again");
    auto* pick_btn = new QPushButton("Pick Technique");
    auto* game_btn = new QPushButton("Return to Game");
    btn_layout->addWidget(again_btn);
    btn_layout->addWidget(pick_btn);
    btn_layout->addWidget(game_btn);
    btn_layout->addStretch();
    layout->addLayout(btn_layout);
    layout->addStretch();

    connect(again_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->selectTechnique(training_vm_->trainingState.get().current_technique);
        }
    });
    connect(pick_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->returnToSelection();
        }
    });
    connect(game_btn, &QPushButton::clicked, this, [this]() {
        if (training_vm_) {
            training_vm_->returnToSelection();
        }
        emit backToGame();
    });

    connect(pages_, &QStackedWidget::currentChanged, this, [this, info_label, verdict_label](int index) {
        if (index != 4 || !training_vm_) {
            return;
        }
        const auto& state = training_vm_->trainingState.get();
        auto name = core::getTechniqueName(state.current_technique);

        info_label->setText(QString("Technique: %1\nScore: %2 / %3 correct\nHints used: %4")
                                .arg(QString::fromUtf8(name.data(), static_cast<int>(name.size())))
                                .arg(state.correct_count)
                                .arg(state.total_exercises)
                                .arg(state.hints_used));

        float ratio = state.total_exercises > 0
                          ? static_cast<float>(state.correct_count) / static_cast<float>(state.total_exercises)
                          : 0.0f;
        if (ratio >= 0.8f) {
            verdict_label->setText("Excellent! You've mastered this technique.");
            verdict_label->setStyleSheet("color: green; font-weight: bold;");
        } else if (ratio >= 0.5f) {
            verdict_label->setText("Good progress. Try again for a higher score.");
            verdict_label->setStyleSheet("color: #cc9900; font-weight: bold;");
        } else {
            verdict_label->setText("Keep practicing! Review the theory and try again.");
            verdict_label->setStyleSheet("color: red; font-weight: bold;");
        }
    });

    pages_->addWidget(page);
}

}  // namespace sudoku::view
