// sudoku - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "core/settings_manager.h"
#include "test_fixture.h"
#include "view/keyboard_shortcuts.h"
#include "view/main_window.h"
#include "view/sudoku_board_widget.h"

#include <chrono>
#include <filesystem>
#include <memory>

#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTest>

using namespace sudoku;

// Story 8-21: the enable_cell_coloring setting, end-to-end through the real
// SettingsManager -> GameViewModel::applySettings / MainWindow::applySettings single apply
// paths (AC8). UITestContext's settings_manager constructor param (added for this story) is
// what makes the ViewModel half reachable at all — without it, a UI test flipping the setting
// would only ever move MainWindow's own labels and the assertions below would be meaningless.
class TestOptionalColoring : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void disablingRestrictsSpaceCycleToNormalAndNotes();
    void disablingMakesAltDigitInert();
    void modeButtonAndHintTrackTheFlag();
    void shortcutsDialogHasTwoFewerRowsWhenDisabled();
    void startupWithPersistedFalseNeverReachesColor();
    void reEnablingRestoresTheCycle();

    // Qt needs `private slots:` for the test methods, kept separate from the plain-`private:`
    // data members below.
    // NOLINTNEXTLINE(readability-redundant-access-specifiers)
private:
    std::filesystem::path settings_dir_;
    std::filesystem::path settings_file_;
    std::shared_ptr<core::SettingsManager> settings_;
    std::unique_ptr<test::UITestContext> ctx_;
    std::unique_ptr<view::MainWindow> window_;

    [[nodiscard]] core::Position selectEmptyCell() const;
};

void TestOptionalColoring::initTestCase() {
    settings_dir_ =
        std::filesystem::temp_directory_path() /
        ("ui_test_optional_coloring_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(settings_dir_);
    settings_file_ = settings_dir_ / "settings.yaml";
}

void TestOptionalColoring::cleanupTestCase() {
    window_.reset();
    ctx_.reset();
    settings_.reset();
    if (std::filesystem::exists(settings_dir_)) {
        std::filesystem::remove_all(settings_dir_);
    }
}

// Fresh SettingsManager + UITestContext + MainWindow per case: the startup-direction case needs
// a MainWindow built AFTER the persisted value is already on disk, which a shared fixture cannot
// give once other cases have already toggled the flag live.
void TestOptionalColoring::init() {
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

core::Position TestOptionalColoring::selectEmptyCell() const {
    const auto& state = ctx_->game_vm->gameState.get();
    for (size_t r = 0; r < core::BOARD_SIZE; ++r) {
        for (size_t c = 0; c < core::BOARD_SIZE; ++c) {
            const auto& cell = state.getCell(r, c);
            if (cell.value == 0 && !cell.is_given) {
                const core::Position pos{.row = r, .col = c};
                window_->board_widget_->setSelectedCell(pos);
                QApplication::processEvents();
                return pos;
            }
        }
    }
    QTest::qFail("No empty cell found on Easy board", __FILE__, __LINE__);
    return core::Position{.row = 0, .col = 0};
}

void TestOptionalColoring::disablingRestrictsSpaceCycleToNormalAndNotes() {
    QCOMPARE(ctx_->game_vm->getInputMode(), viewmodel::InputMode::Normal);

    settings_->setEnableCellColoring(false);
    QApplication::processEvents();

    QTest::keyClick(window_->board_widget_, Qt::Key_Space);
    QApplication::processEvents();
    QCOMPARE(ctx_->game_vm->getInputMode(), viewmodel::InputMode::Notes);

    QTest::keyClick(window_->board_widget_, Qt::Key_Space);
    QApplication::processEvents();
    QCOMPARE(ctx_->game_vm->getInputMode(), viewmodel::InputMode::Normal);
}

void TestOptionalColoring::disablingMakesAltDigitInert() {
    const auto pos = selectEmptyCell();

    settings_->setEnableCellColoring(false);
    QApplication::processEvents();

    QTest::keyClick(window_->board_widget_, Qt::Key_3, Qt::AltModifier);
    QApplication::processEvents();

    QCOMPARE(ctx_->game_vm->gameState.get().getCellColor(pos.row, pos.col), static_cast<uint8_t>(0));
}

void TestOptionalColoring::modeButtonAndHintTrackTheFlag() {
    auto* hint = window_->findChild<QLabel*>("modifierHintLabel");
    QVERIFY(hint != nullptr);
    if (hint == nullptr) {
        return;
    }
    QVERIFY(hint->text().contains(view::nativeModifierName(Qt::AltModifier)));

    settings_->setEnableCellColoring(false);
    QApplication::processEvents();

    QVERIFY(!window_->mode_btn_->text().contains(QStringLiteral("Color")));
    QVERIFY(!hint->text().contains(view::nativeModifierName(Qt::AltModifier)));

    settings_->setEnableCellColoring(true);
    QApplication::processEvents();

    QVERIFY(hint->text().contains(view::nativeModifierName(Qt::AltModifier)));
}

void TestOptionalColoring::shortcutsDialogHasTwoFewerRowsWhenDisabled() {
    auto* enabled_dialog = window_->buildKeyboardShortcutsDialog();
    QVERIFY(enabled_dialog != nullptr);
    auto* enabled_table = enabled_dialog->findChild<QTableWidget*>("keyboardShortcutsTable");
    QVERIFY(enabled_table != nullptr);
    const int enabled_rows = enabled_table != nullptr ? enabled_table->rowCount() : 0;
    delete enabled_dialog;

    settings_->setEnableCellColoring(false);
    QApplication::processEvents();

    auto* disabled_dialog = window_->buildKeyboardShortcutsDialog();
    QVERIFY(disabled_dialog != nullptr);
    auto* disabled_table = disabled_dialog->findChild<QTableWidget*>("keyboardShortcutsTable");
    QVERIFY(disabled_table != nullptr);
    if (disabled_table != nullptr) {
        QCOMPARE(disabled_table->rowCount(), enabled_rows - 2);
    }
    delete disabled_dialog;
}

// D1-shaped regression (story 8-19's lesson, reapplied here): a MainWindow built AFTER the
// persisted value is already false must gate from the very first Space press — not only after a
// live toggle. Building the window before flipping the flag (as the four test_session_timer
// cases do) cannot catch a missing initial apply.
void TestOptionalColoring::startupWithPersistedFalseNeverReachesColor() {
    settings_->setEnableCellColoring(false);

    auto fresh_settings = std::make_shared<core::SettingsManager>(settings_file_);
    QVERIFY(!fresh_settings->getSettings().enable_cell_coloring);

    auto fresh_ctx = std::make_unique<test::UITestContext>(fresh_settings);
    view::MainWindow fresh_window;
    fresh_ctx->setupMainWindow(fresh_window);
    fresh_window.setSettingsManager(fresh_settings);
    fresh_window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&fresh_window));

    fresh_ctx->game_vm->startNewGame(core::Difficulty::Easy);
    QApplication::processEvents();

    QTest::keyClick(fresh_window.board_widget_, Qt::Key_Space);
    QApplication::processEvents();
    QCOMPARE(fresh_ctx->game_vm->getInputMode(), viewmodel::InputMode::Notes);

    QTest::keyClick(fresh_window.board_widget_, Qt::Key_Space);
    QApplication::processEvents();
    QCOMPARE(fresh_ctx->game_vm->getInputMode(), viewmodel::InputMode::Normal);
}

void TestOptionalColoring::reEnablingRestoresTheCycle() {
    settings_->setEnableCellColoring(false);
    QApplication::processEvents();
    QTest::keyClick(window_->board_widget_, Qt::Key_Space);
    QApplication::processEvents();
    QCOMPARE(ctx_->game_vm->getInputMode(), viewmodel::InputMode::Notes);

    settings_->setEnableCellColoring(true);
    QApplication::processEvents();

    QTest::keyClick(window_->board_widget_, Qt::Key_Space);
    QApplication::processEvents();
    QCOMPARE(ctx_->game_vm->getInputMode(), viewmodel::InputMode::Color);
}

QTEST_MAIN(TestOptionalColoring)
#include "test_optional_coloring.moc"
