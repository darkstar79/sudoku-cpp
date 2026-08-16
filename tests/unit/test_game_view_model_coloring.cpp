// sudoku - Offline Sudoku Game
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

#include "../helpers/game_view_model_fixture.h"
#include "../helpers/test_utils.h"

#include <optional>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::viewmodel;
using namespace sudoku::core;

using ColoringFixture = sudoku::test::GameViewModelFixture;

// Default (no ISettingsManager, the shape every existing tests/unit fixture builds — AC2's
// back-compat guarantee) leaves coloring enabled and the cycle reaching Color unchanged.
TEST_CASE("GameViewModel - Coloring defaults enabled with no settings manager", "[game_view_model][coloring]") {
    ColoringFixture fixture;
    fixture.view_model->startNewGame(Difficulty::Easy);
    auto& vm = *fixture.view_model;

    REQUIRE(vm.isColoringEnabled());

    vm.cycleInputMode();
    REQUIRE(vm.getInputMode() == InputMode::Notes);
    vm.cycleInputMode();
    REQUIRE(vm.getInputMode() == InputMode::Color);
}

TEST_CASE("GameViewModel - Disabling coloring keeps the cycle to Normal/Notes only", "[game_view_model][coloring]") {
    ColoringFixture fixture;
    fixture.view_model->startNewGame(Difficulty::Easy);
    auto& vm = *fixture.view_model;

    vm.setColoringEnabled(false);
    REQUIRE_FALSE(vm.isColoringEnabled());
    REQUIRE(vm.getInputMode() == InputMode::Normal);

    // Across >= 3 presses, never Color — a fix that only handles the first hop must not pass.
    vm.cycleInputMode();
    REQUIRE(vm.getInputMode() == InputMode::Notes);
    vm.cycleInputMode();
    REQUIRE(vm.getInputMode() == InputMode::Normal);
    vm.cycleInputMode();
    REQUIRE(vm.getInputMode() == InputMode::Notes);
}

TEST_CASE("GameViewModel - Disabled coloring makes every color entry point inert", "[game_view_model][coloring]") {
    ColoringFixture fixture;
    fixture.view_model->startNewGame(Difficulty::Easy);
    auto& vm = *fixture.view_model;

    auto empty_opt = test::findEmptyCell(vm.gameState.get());
    REQUIRE(empty_opt.has_value());
    const auto pos = empty_opt.value_or(Position{});

    vm.setColoringEnabled(false);

    SECTION("plain Color-mode digit (Alt override) is a silent no-op") {
        vm.handleNumberInput(pos, 3, InputMode::Color);
        REQUIRE(vm.gameState.get().getCellColor(pos.row, pos.col) == 0);
    }

    SECTION("plain digit while input_mode == Color is a silent no-op") {
        // setColoringEnabled(false) already left Color mode (AC5) — force it back to simulate a
        // stale InputMode reaching handleNumberInput (e.g. a caller that sets mode directly,
        // bypassing the cycle). The Color case's own gate must still make this inert.
        vm.setInputMode(InputMode::Color);
        REQUIRE(vm.getInputMode() == InputMode::Color);

        vm.handleNumberInput(pos, 3, std::nullopt);

        REQUIRE(vm.gameState.get().getCellColor(pos.row, pos.col) == 0);
        REQUIRE(vm.gameState.get().getCell(pos).value == 0);
    }

    SECTION("colorCell called directly is a silent no-op") {
        vm.colorCell(pos, 3);
        REQUIRE(vm.gameState.get().getCellColor(pos.row, pos.col) == 0);
    }
}

TEST_CASE("GameViewModel - Disabled coloring leaves the value and pencil layers untouched",
          "[game_view_model][coloring]") {
    ColoringFixture fixture;
    fixture.view_model->startNewGame(Difficulty::Easy);
    auto& vm = *fixture.view_model;

    auto empty_opt = test::findEmptyCell(vm.gameState.get());
    REQUIRE(empty_opt.has_value());
    const auto pos = empty_opt.value_or(Position{});

    vm.setColoringEnabled(false);

    SECTION("Ctrl override still places a value") {
        vm.handleNumberInput(pos, 5, InputMode::Normal);
        REQUIRE(vm.gameState.get().getCell(pos).value == 5);
    }

    SECTION("Shift override still toggles a pencil mark") {
        vm.handleNumberInput(pos, 3, InputMode::Notes);
        REQUIRE(vm.gameState.get().getCell(pos).notes.contains(3));
    }
}

TEST_CASE("GameViewModel - Disabling coloring mid-game clears colors and re-enabling does not restore them",
          "[game_view_model][coloring]") {
    ColoringFixture fixture;
    fixture.view_model->startNewGame(Difficulty::Easy);
    auto& vm = *fixture.view_model;

    auto cells_opt = test::findEmptyCells(vm.gameState.get(), 2);
    REQUIRE(cells_opt.has_value());
    const auto cells = cells_opt.value_or(std::vector<Position>{});
    REQUIRE(cells.size() == 2);

    vm.colorCell(cells[0], 2);
    vm.colorCell(cells[1], 4);
    REQUIRE(vm.gameState.get().getCellColor(cells[0].row, cells[0].col) == 2);
    REQUIRE(vm.gameState.get().getCellColor(cells[1].row, cells[1].col) == 4);
    vm.setInputMode(InputMode::Color);

    vm.setColoringEnabled(false);

    REQUIRE(vm.getInputMode() == InputMode::Normal);
    REQUIRE(vm.gameState.get().getCellColor(cells[0].row, cells[0].col) == 0);
    REQUIRE(vm.gameState.get().getCellColor(cells[1].row, cells[1].col) == 0);

    vm.setColoringEnabled(true);
    REQUIRE(vm.isColoringEnabled());
    vm.cycleInputMode();
    vm.cycleInputMode();
    REQUIRE(vm.getInputMode() == InputMode::Color);
    // Colors cleared by the disable do not come back.
    REQUIRE(vm.gameState.get().getCellColor(cells[0].row, cells[0].col) == 0);
    REQUIRE(vm.gameState.get().getCellColor(cells[1].row, cells[1].col) == 0);
}

TEST_CASE("GameViewModel - setColoringEnabled is idempotent when already at the target value",
          "[game_view_model][coloring]") {
    ColoringFixture fixture;
    fixture.view_model->startNewGame(Difficulty::Easy);
    auto& vm = *fixture.view_model;

    auto empty_opt = test::findEmptyCell(vm.gameState.get());
    REQUIRE(empty_opt.has_value());
    const auto pos = empty_opt.value_or(Position{});

    // Guards against a stray clearAllCellColors() firing on every settings notification, even
    // when the value did not actually change.
    vm.colorCell(pos, 3);
    REQUIRE(vm.gameState.get().getCellColor(pos.row, pos.col) == 3);

    vm.setColoringEnabled(true);  // already enabled — no-op

    REQUIRE(vm.gameState.get().getCellColor(pos.row, pos.col) == 3);
}
