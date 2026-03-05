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

#include "../../src/core/solving_technique.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("SolvingTechnique - Enum Values", "[solving_technique]") {
    SECTION("All technique types are defined") {
        // Verify all enum values exist (all 13 techniques)
        auto naked_single = SolvingTechnique::NakedSingle;
        auto hidden_single = SolvingTechnique::HiddenSingle;
        auto naked_pair = SolvingTechnique::NakedPair;
        auto naked_triple = SolvingTechnique::NakedTriple;
        auto hidden_pair = SolvingTechnique::HiddenPair;
        auto hidden_triple = SolvingTechnique::HiddenTriple;
        auto pointing_pair = SolvingTechnique::PointingPair;
        auto box_line_reduction = SolvingTechnique::BoxLineReduction;
        auto naked_quad = SolvingTechnique::NakedQuad;
        auto hidden_quad = SolvingTechnique::HiddenQuad;
        auto x_wing = SolvingTechnique::XWing;
        auto xy_wing = SolvingTechnique::XYWing;
        auto backtracking = SolvingTechnique::Backtracking;

        // Suppress unused variable warnings
        (void)naked_single;
        (void)hidden_single;
        (void)naked_pair;
        (void)naked_triple;
        (void)hidden_pair;
        (void)hidden_triple;
        (void)pointing_pair;
        (void)box_line_reduction;
        (void)naked_quad;
        (void)hidden_quad;
        (void)x_wing;
        (void)xy_wing;
        (void)backtracking;

        REQUIRE(true);  // Compilation success proves enum exists
    }

    SECTION("Underlying type is uint8_t") {
        // Verify enum uses correct underlying type for memory efficiency
        REQUIRE(sizeof(SolvingTechnique) == sizeof(uint8_t));
    }
}

TEST_CASE("SolvingTechnique - getTechniqueName()", "[solving_technique]") {
    SECTION("Returns correct names for all techniques") {
        REQUIRE(getTechniqueName(SolvingTechnique::NakedSingle) == "Naked Single");
        REQUIRE(getTechniqueName(SolvingTechnique::HiddenSingle) == "Hidden Single");
        REQUIRE(getTechniqueName(SolvingTechnique::NakedPair) == "Naked Pair");
        REQUIRE(getTechniqueName(SolvingTechnique::NakedTriple) == "Naked Triple");
        REQUIRE(getTechniqueName(SolvingTechnique::HiddenPair) == "Hidden Pair");
        REQUIRE(getTechniqueName(SolvingTechnique::HiddenTriple) == "Hidden Triple");
        REQUIRE(getTechniqueName(SolvingTechnique::PointingPair) == "Pointing Pair");
        REQUIRE(getTechniqueName(SolvingTechnique::BoxLineReduction) == "Box/Line Reduction");
        REQUIRE(getTechniqueName(SolvingTechnique::NakedQuad) == "Naked Quad");
        REQUIRE(getTechniqueName(SolvingTechnique::HiddenQuad) == "Hidden Quad");
        REQUIRE(getTechniqueName(SolvingTechnique::XWing) == "X-Wing");
        REQUIRE(getTechniqueName(SolvingTechnique::XYWing) == "XY-Wing");
        REQUIRE(getTechniqueName(SolvingTechnique::Backtracking) == "Backtracking");
    }

    SECTION("Names are non-empty") {
        // Test all 13 techniques consistently
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::NakedSingle).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::HiddenSingle).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::NakedPair).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::NakedTriple).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::HiddenPair).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::HiddenTriple).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::PointingPair).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::BoxLineReduction).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::NakedQuad).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::HiddenQuad).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::XWing).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::XYWing).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::Backtracking).empty());
    }
}

TEST_CASE("SolvingTechnique - getTechniquePoints()", "[solving_technique]") {
    SECTION("Returns correct Sudoku Explainer point values") {
        // All technique point values (per Sudoku Explainer scale)
        REQUIRE(getTechniquePoints(SolvingTechnique::NakedSingle) == 10);
        REQUIRE(getTechniquePoints(SolvingTechnique::HiddenSingle) == 15);
        REQUIRE(getTechniquePoints(SolvingTechnique::NakedPair) == 50);
        REQUIRE(getTechniquePoints(SolvingTechnique::NakedTriple) == 60);
        REQUIRE(getTechniquePoints(SolvingTechnique::HiddenPair) == 70);
        REQUIRE(getTechniquePoints(SolvingTechnique::HiddenTriple) == 90);
        REQUIRE(getTechniquePoints(SolvingTechnique::PointingPair) == 100);
        REQUIRE(getTechniquePoints(SolvingTechnique::BoxLineReduction) == 100);
        REQUIRE(getTechniquePoints(SolvingTechnique::NakedQuad) == 120);
        REQUIRE(getTechniquePoints(SolvingTechnique::HiddenQuad) == 150);
        REQUIRE(getTechniquePoints(SolvingTechnique::XWing) == 200);
        REQUIRE(getTechniquePoints(SolvingTechnique::XYWing) == 200);
    }

    SECTION("Backtracking is rated as harder than logical strategies") {
        // Backtracking means no logical technique could solve this step
        REQUIRE(getTechniquePoints(SolvingTechnique::Backtracking) == 750);
    }

    SECTION("Point values are ordered by difficulty") {
        // Verify techniques are ordered easiest to hardest
        REQUIRE(getTechniquePoints(SolvingTechnique::NakedSingle) < getTechniquePoints(SolvingTechnique::HiddenSingle));
        REQUIRE(getTechniquePoints(SolvingTechnique::HiddenSingle) < getTechniquePoints(SolvingTechnique::NakedPair));
        REQUIRE(getTechniquePoints(SolvingTechnique::NakedPair) < getTechniquePoints(SolvingTechnique::NakedTriple));
        REQUIRE(getTechniquePoints(SolvingTechnique::NakedTriple) < getTechniquePoints(SolvingTechnique::HiddenPair));
        REQUIRE(getTechniquePoints(SolvingTechnique::HiddenPair) < getTechniquePoints(SolvingTechnique::HiddenTriple));
        REQUIRE(getTechniquePoints(SolvingTechnique::HiddenTriple) <
                getTechniquePoints(SolvingTechnique::PointingPair));
        REQUIRE(getTechniquePoints(SolvingTechnique::PointingPair) ==
                getTechniquePoints(SolvingTechnique::BoxLineReduction));
        REQUIRE(getTechniquePoints(SolvingTechnique::BoxLineReduction) <
                getTechniquePoints(SolvingTechnique::NakedQuad));
        REQUIRE(getTechniquePoints(SolvingTechnique::NakedQuad) < getTechniquePoints(SolvingTechnique::HiddenQuad));
        REQUIRE(getTechniquePoints(SolvingTechnique::HiddenQuad) < getTechniquePoints(SolvingTechnique::XWing));
        REQUIRE(getTechniquePoints(SolvingTechnique::XWing) == getTechniquePoints(SolvingTechnique::XYWing));
    }

    SECTION("All point values are non-negative") {
        REQUIRE(getTechniquePoints(SolvingTechnique::NakedSingle) >= 0);
        REQUIRE(getTechniquePoints(SolvingTechnique::HiddenSingle) >= 0);
        REQUIRE(getTechniquePoints(SolvingTechnique::NakedPair) >= 0);
        REQUIRE(getTechniquePoints(SolvingTechnique::NakedTriple) >= 0);
        REQUIRE(getTechniquePoints(SolvingTechnique::HiddenPair) >= 0);
        REQUIRE(getTechniquePoints(SolvingTechnique::HiddenTriple) >= 0);
        REQUIRE(getTechniquePoints(SolvingTechnique::PointingPair) >= 0);
        REQUIRE(getTechniquePoints(SolvingTechnique::BoxLineReduction) >= 0);
        REQUIRE(getTechniquePoints(SolvingTechnique::NakedQuad) >= 0);
        REQUIRE(getTechniquePoints(SolvingTechnique::HiddenQuad) >= 0);
        REQUIRE(getTechniquePoints(SolvingTechnique::XWing) >= 0);
        REQUIRE(getTechniquePoints(SolvingTechnique::XYWing) >= 0);
        REQUIRE(getTechniquePoints(SolvingTechnique::Backtracking) >= 0);
    }
}

TEST_CASE("SolvingTechnique - Helper Functions are Constexpr", "[solving_technique]") {
    SECTION("getTechniqueName() is constexpr") {
        // Verify function can be evaluated at compile time
        constexpr auto name = getTechniqueName(SolvingTechnique::NakedSingle);
        REQUIRE(name == "Naked Single");
    }

    SECTION("getTechniquePoints() is constexpr") {
        // Verify function can be evaluated at compile time
        constexpr int points = getTechniquePoints(SolvingTechnique::NakedSingle);
        REQUIRE(points == 10);
    }
}
