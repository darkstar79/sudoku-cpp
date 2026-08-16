// sudoku - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "core/board_data.h"
#include "core/settings_manager.h"
#include "core/solving_technique.h"
#include "test_fixture.h"
#include "view/main_window.h"
#include "view/sudoku_board_widget.h"

#include <chrono>
#include <filesystem>
#include <memory>

#include <QApplication>
#include <QLabel>
#include <QTest>
#include <QWidget>

using namespace sudoku;

namespace {

[[nodiscard]] int countFilledCells(const core::BoardData& board) {
    int filled = 0;
    for (const auto row : board) {
        for (int cell : row) {
            filled += (cell != 0) ? 1 : 0;
        }
    }
    return filled;
}

}  // namespace

// Story 8-22: GameViewModel::hintMessage is computed for every hint (basic and find-by-technique)
// but nothing in src/view ever subscribed to it (D1) — hints revealed a digit with no explanation.
// "Show Hints" (Settings -> Display) was also write-only (D2). These tests assert the *rendered*
// widget, not the observable — ctx_->game_vm->hintMessage.get() is already green today, which is
// exactly the bug (see tests/ui/test_find_by_technique.cpp:151 for the pre-existing observable-only
// assertion this story does not replace, only supplements at the View level).
class TestHintDisplay : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void hintTakenShowsExplanationPanel();
    void findByTechniqueAlsoShowsExplanationPanel();
    void newGameClearsExplanationPanel();
    void showHintsFalseHidesExplanationButKeepsHintWorking();
    void showHintsTrueShowsExplanation();
    void startupWithPersistedShowHintsFalseNeverShowsExplanation();
    void togglingShowHintsLiveAffectsWithoutRestart();
    void coachingActiveHidesLeftoverBasicHintPanel();
    void basicHintDismissesActiveCoachingPanel();

    // NOLINTNEXTLINE(readability-redundant-access-specifiers)
private:
    std::filesystem::path settings_dir_;
    std::filesystem::path settings_file_;
    std::shared_ptr<core::SettingsManager> settings_;
    std::unique_ptr<test::UITestContext> ctx_;
    std::unique_ptr<view::MainWindow> window_;

    [[nodiscard]] core::Position selectEmptyCellOn(view::MainWindow& window,
                                                   const std::shared_ptr<viewmodel::GameViewModel>& vm) const;
};

void TestHintDisplay::initTestCase() {
    settings_dir_ =
        std::filesystem::temp_directory_path() /
        ("ui_test_hint_display_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(settings_dir_);
    settings_file_ = settings_dir_ / "settings.yaml";
}

void TestHintDisplay::cleanupTestCase() {
    window_.reset();
    ctx_.reset();
    settings_.reset();
    if (std::filesystem::exists(settings_dir_)) {
        std::filesystem::remove_all(settings_dir_);
    }
}

// Fresh SettingsManager + UITestContext + MainWindow per case: the startup-direction case needs a
// MainWindow built AFTER the persisted value is already on disk, which a shared fixture cannot give
// once other cases have already toggled the flag live (same rationale as test_optional_coloring.cpp).
void TestHintDisplay::init() {
    if (std::filesystem::exists(settings_file_)) {
        std::filesystem::remove(settings_file_);
    }
    settings_ = std::make_shared<core::SettingsManager>(settings_file_);
    ctx_ = std::make_unique<test::UITestContext>(settings_);
    window_ = std::make_unique<view::MainWindow>();
    ctx_->setupMainWindow(*window_);
    window_->setSettingsManager(settings_);
    window_->show();
    QVERIFY(QTest::qWaitForWindowExposed(window_.get()));

    ctx_->game_vm->startNewGame(core::Difficulty::Easy);
    QApplication::processEvents();
}

core::Position TestHintDisplay::selectEmptyCellOn(view::MainWindow& window,
                                                  const std::shared_ptr<viewmodel::GameViewModel>& vm) const {
    const auto& state = vm->gameState.get();
    for (size_t r = 0; r < core::BOARD_SIZE; ++r) {
        for (size_t c = 0; c < core::BOARD_SIZE; ++c) {
            const auto& cell = state.getCell(r, c);
            if (cell.value == 0 && !cell.is_given) {
                const core::Position pos{.row = r, .col = c};
                window.board_widget_->setSelectedCell(pos);
                QApplication::processEvents();
                return pos;
            }
        }
    }
    QTest::qFail("No empty cell found on Easy board", __FILE__, __LINE__);
    return core::Position{.row = 0, .col = 0};
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void TestHintDisplay::hintTakenShowsExplanationPanel() {
    auto* panel = window_->findChild<QWidget*>("hintExplanationPanel");
    auto* label = window_->findChild<QLabel*>("hintExplanationLabel");
    QVERIFY(panel != nullptr);
    QVERIFY(label != nullptr);
    if (panel == nullptr || label == nullptr) {
        return;
    }
    QVERIFY(!panel->isVisible());

    const auto pos = selectEmptyCellOn(*window_, ctx_->game_vm);
    ctx_->game_vm->getHint(pos);
    QApplication::processEvents();

    QVERIFY2(panel->isVisible(), "Taking a hint should reveal the explanation panel");
    QVERIFY2(!label->text().isEmpty(), "The explanation label should carry the hint's explanation text");
}

void TestHintDisplay::findByTechniqueAlsoShowsExplanationPanel() {
    auto* panel = window_->findChild<QWidget*>("hintExplanationPanel");
    QVERIFY(panel != nullptr);
    if (panel == nullptr) {
        return;
    }
    QVERIFY(!panel->isVisible());

    ctx_->game_vm->findStepByTechnique(core::SolvingTechnique::NakedSingle);
    QApplication::processEvents();

    QVERIFY2(panel->isVisible(), "Find Step by Technique should also reach the explanation panel (AC3)");
}

void TestHintDisplay::newGameClearsExplanationPanel() {
    auto* panel = window_->findChild<QWidget*>("hintExplanationPanel");
    QVERIFY(panel != nullptr);
    if (panel == nullptr) {
        return;
    }

    const auto pos = selectEmptyCellOn(*window_, ctx_->game_vm);
    ctx_->game_vm->getHint(pos);
    QApplication::processEvents();
    QVERIFY(panel->isVisible());

    ctx_->game_vm->startNewGame(core::Difficulty::Easy);
    QApplication::processEvents();

    QVERIFY2(!panel->isVisible(), "A fresh game clears hintMessage, which must hide the panel (AC4)");
}

void TestHintDisplay::showHintsFalseHidesExplanationButKeepsHintWorking() {
    settings_->setShowHints(false);
    QApplication::processEvents();

    auto* panel = window_->findChild<QWidget*>("hintExplanationPanel");
    QVERIFY(panel != nullptr);
    if (panel == nullptr) {
        return;
    }

    const auto pos = selectEmptyCellOn(*window_, ctx_->game_vm);
    const int hints_before = ctx_->game_vm->getHintCount();
    const int filled_before = countFilledCells(ctx_->game_vm->gameState.get().extractNumbers());

    ctx_->game_vm->getHint(pos);
    QApplication::processEvents();

    QVERIFY2(!panel->isVisible(), "show_hints == false must suppress the explanation panel (AC5)");
    // The revealed digit lands wherever findNextStep's chosen step is, not necessarily the
    // selected cell (a known, out-of-scope open question for this story) — so assert one more
    // cell got filled in, not that `pos` specifically did.
    const int filled_after = countFilledCells(ctx_->game_vm->gameState.get().extractNumbers());
    QCOMPARE(filled_after, filled_before + 1);                  // digit still revealed (AC9)
    QCOMPARE(ctx_->game_vm->getHintCount(), hints_before - 1);  // budget still consumed (AC9)
}

void TestHintDisplay::showHintsTrueShowsExplanation() {
    settings_->setShowHints(true);
    QApplication::processEvents();

    auto* panel = window_->findChild<QWidget*>("hintExplanationPanel");
    QVERIFY(panel != nullptr);
    if (panel == nullptr) {
        return;
    }

    const auto pos = selectEmptyCellOn(*window_, ctx_->game_vm);
    ctx_->game_vm->getHint(pos);
    QApplication::processEvents();

    QVERIFY2(panel->isVisible(), "show_hints == true (the default) must show the explanation panel (AC5)");
}

// D1-shaped regression (story 8-19/8-21's lesson, reapplied here): a MainWindow built AFTER the
// persisted value is already false must gate from the very first hint — not only after a live
// toggle. Building the window before flipping the flag (as most cases in this file do via init())
// cannot catch a missing initial apply.
void TestHintDisplay::startupWithPersistedShowHintsFalseNeverShowsExplanation() {
    settings_->setShowHints(false);

    auto fresh_settings = std::make_shared<core::SettingsManager>(settings_file_);
    QVERIFY(!fresh_settings->getSettings().show_hints);

    auto fresh_ctx = std::make_unique<test::UITestContext>(fresh_settings);
    view::MainWindow fresh_window;
    fresh_ctx->setupMainWindow(fresh_window);
    fresh_window.setSettingsManager(fresh_settings);
    fresh_window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&fresh_window));

    fresh_ctx->game_vm->startNewGame(core::Difficulty::Easy);
    QApplication::processEvents();

    auto* panel = fresh_window.findChild<QWidget*>("hintExplanationPanel");
    QVERIFY(panel != nullptr);
    if (panel == nullptr) {
        return;
    }

    const auto pos = selectEmptyCellOn(fresh_window, fresh_ctx->game_vm);
    fresh_ctx->game_vm->getHint(pos);
    QApplication::processEvents();

    QVERIFY2(!panel->isVisible(), "A persisted show_hints=false must gate the very first hint after launch (AC6)");
}

void TestHintDisplay::togglingShowHintsLiveAffectsWithoutRestart() {
    auto* panel = window_->findChild<QWidget*>("hintExplanationPanel");
    QVERIFY(panel != nullptr);
    if (panel == nullptr) {
        return;
    }

    settings_->setShowHints(false);
    QApplication::processEvents();
    auto pos = selectEmptyCellOn(*window_, ctx_->game_vm);
    ctx_->game_vm->getHint(pos);
    QApplication::processEvents();
    QVERIFY(!panel->isVisible());

    settings_->setShowHints(true);
    QApplication::processEvents();
    pos = selectEmptyCellOn(*window_, ctx_->game_vm);
    ctx_->game_vm->getHint(pos);
    QApplication::processEvents();
    QVERIFY2(panel->isVisible(), "Toggling the checkbox must take effect on the next hint without a restart (AC6)");
}

// Enumerates one of the two coaching/basic-hint orderings (landmine in the story's Dev Notes): a
// stale basic-hint explanation must not sit next to an active coaching panel.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void TestHintDisplay::coachingActiveHidesLeftoverBasicHintPanel() {
    auto* hint_panel = window_->findChild<QWidget*>("hintExplanationPanel");
    QVERIFY(hint_panel != nullptr);
    if (hint_panel == nullptr) {
        return;
    }

    const auto pos = selectEmptyCellOn(*window_, ctx_->game_vm);
    ctx_->game_vm->getHint(pos);
    QApplication::processEvents();
    QVERIFY(hint_panel->isVisible());

    ctx_->game_vm->requestCoachingHint();
    QApplication::processEvents();

    QVERIFY2(!hint_panel->isVisible(),
             "A leftover basic-hint explanation must not remain visible once coaching becomes active (AC8)");
    QVERIFY2(ctx_->game_vm->coachingState.get().active, "Coaching should have become active");
}

// The other ordering: a basic hint requested mid-coaching must not blank/replace coaching's own
// message (they are independent widgets — AC8) and must end up showing its own explanation, since
// getHint() calls resetCoachingState() before publishing hintMessage.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void TestHintDisplay::basicHintDismissesActiveCoachingPanel() {
    auto* hint_panel = window_->findChild<QWidget*>("hintExplanationPanel");
    QVERIFY(hint_panel != nullptr);
    if (hint_panel == nullptr) {
        return;
    }

    ctx_->game_vm->requestCoachingHint();
    QApplication::processEvents();
    QVERIFY(ctx_->game_vm->coachingState.get().active);

    const auto pos = selectEmptyCellOn(*window_, ctx_->game_vm);
    ctx_->game_vm->getHint(pos);
    QApplication::processEvents();

    QVERIFY2(!ctx_->game_vm->coachingState.get().active, "getHint() ends coaching via resetCoachingState()");
    QVERIFY2(hint_panel->isVisible(), "The basic hint's own explanation should now be showing");
}

QTEST_MAIN(TestHintDisplay)
#include "test_hint_display.moc"
