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

/// Branch-coverage tests for MockLocalizationManager:
/// - setLocale (lines 21-22)
/// - getCurrentLocale (lines 25-26)
/// - getAvailableLocales (lines 29-31)

#include "../../src/core/mock_localization_manager.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("MockLocalizationManager - getString returns key", "[mock_localization]") {
    MockLocalizationManager mock;
    REQUIRE(std::string_view(mock.getString("sudoku.difficulty.easy")) == "sudoku.difficulty.easy");
    REQUIRE(std::string_view(mock.getString("some.key")) == "some.key");
}

TEST_CASE("MockLocalizationManager - setLocale returns success", "[mock_localization]") {
    // Covers lines 21-22: setLocale() always returns {}
    MockLocalizationManager mock;
    auto result = mock.setLocale("fr");
    REQUIRE(result.has_value());
}

TEST_CASE("MockLocalizationManager - getCurrentLocale returns en", "[mock_localization]") {
    // Covers lines 25-26: getCurrentLocale() always returns "en"
    MockLocalizationManager mock;
    REQUIRE(mock.getCurrentLocale() == "en");
}

TEST_CASE("MockLocalizationManager - getAvailableLocales returns en entry", "[mock_localization]") {
    // Covers lines 29-31: getAvailableLocales() returns {{"en", "English"}}
    MockLocalizationManager mock;
    auto locales = mock.getAvailableLocales();
    REQUIRE(locales.size() == 1);
    REQUIRE(locales[0].first == "en");
    REQUIRE(locales[0].second == "English");
}
