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

#include "i_localization_manager.h"
#include "string_keys.h"

#include <cstdint>
#include <string_view>

namespace sudoku::core {

/// Sudoku solving techniques ordered by difficulty
/// Points based on Sudoku Explainer rating scale
enum class SolvingTechnique : uint8_t {
    NakedSingle = 0,       ///< Only one candidate in cell (10 points)
    HiddenSingle = 1,      ///< Value can only appear in one cell in region (15 points)
    NakedPair = 2,         ///< Two cells with same two candidates (50 points)
    NakedTriple = 3,       ///< Three cells with same three candidates (60 points)
    HiddenPair = 4,        ///< Two values confined to two cells in region (70 points)
    HiddenTriple = 5,      ///< Three values confined to three cells in region (90 points)
    PointingPair = 6,      ///< Candidate in box confined to one row/col → eliminate from rest of row/col (100 points)
    BoxLineReduction = 7,  ///< Candidate in row/col confined to one box → eliminate from rest of box (100 points)
    NakedQuad = 8,         ///< Four cells with same four candidates (120 points)
    HiddenQuad = 9,        ///< Four values confined to four cells in region (150 points)
    XWing = 10,            ///< Candidate in 2 rows appears in same 2 cols → eliminate from cols (200 points)
    XYWing = 11,           ///< Pivot+wings pattern eliminates shared candidate (200 points)
    Swordfish = 12,        ///< Candidate in 3 rows appears in same 3 cols → eliminate from cols (250 points)
    Skyscraper = 13,       ///< Two conjugate pairs sharing one endpoint (250 points)
    TwoStringKite = 14,    ///< Row+column conjugate pairs connected through box (250 points)
    XYZWing = 15,          ///< Pivot(3 cands)+wings eliminate shared candidate (280 points)
    UniqueRectangle = 16,  ///< Avoid deadly pattern in rectangle across 2 boxes (300 points)
    WWing = 17,            ///< Two same bivalue cells connected by strong link (300 points)
    SimpleColoring = 18,   ///< Single-digit conjugate chain coloring (350 points)
    FinnedXWing = 19,      ///< X-Wing with extra candidate cell (fin) (350 points)
    RemotePairs = 20,      ///< Chain of bivalue cells with same pair (350 points)
    BUG = 21,              ///< Bivalue Universal Grave — single trivalue cell placement (350 points)
    Jellyfish = 22,        ///< 4x4 fish pattern (400 points)
    FinnedSwordfish = 23,  ///< Swordfish with extra candidate cell (fin) (400 points)
    EmptyRectangle = 24,   ///< Single-digit chain through box intersection (400 points)
    WXYZWing = 25,         ///< 4-cell wing pattern eliminating shared candidate (400 points)
    FinnedJellyfish = 26,  ///< 4x4 fish with extra fin candidate (450 points)
    XYChain = 27,          ///< Chain of bivalue cells with alternating shared values (450 points)
    MultiColoring = 28,    ///< Cross-cluster single-digit coloring (400 points)
    ALSxZ = 29,            ///< Almost Locked Set XZ-rule (500 points)
    SueDeCoq = 30,         ///< Two-Sector Disjoint Subset (500 points)
    ForcingChain = 31,     ///< Cell forcing chains — assume each candidate, propagate (550 points)
    NiceLoop = 32,         ///< Alternating Inference Chains (600 points)
    XCycles = 33,          ///< Single-digit alternating chain cycles (350 points)
    ThreeDMedusa = 34,     ///< Multi-digit coloring on (cell,digit) pairs (400 points)
    HiddenUniqueRectangle = 35,  ///< Hidden deadly pattern in rectangle (350 points)
    AvoidableRectangle = 36,     ///< Deadly pattern using given/solved distinction (350 points)
    ALSXYWing = 37,              ///< Three ALSs linked by two restricted commons (550 points)
    DeathBlossom = 38,           ///< Stem cell + petal ALSs with common elimination (550 points)
    VWXYZWing = 39,              ///< 5-cell wing pattern with 5 candidate values (450 points)
    FrankenFish = 40,            ///< Fish patterns with mixed row/col/box base+cover sets (450 points)
    GroupedXCycles = 41,         ///< X-Cycles with grouped nodes (pointing pairs) (450 points)
    Backtracking = 255           ///< Not a logical technique - fallback solver (0 points)
};

/// Returns human-readable name for technique
/// @param technique The solving technique
/// @return Technique name (e.g., "Naked Single")
[[nodiscard]] constexpr std::string_view getTechniqueName(SolvingTechnique technique) {
    using enum SolvingTechnique;
    switch (technique) {
        case NakedSingle:
            return "Naked Single";
        case HiddenSingle:
            return "Hidden Single";
        case NakedPair:
            return "Naked Pair";
        case NakedTriple:
            return "Naked Triple";
        case HiddenPair:
            return "Hidden Pair";
        case HiddenTriple:
            return "Hidden Triple";
        case PointingPair:
            return "Pointing Pair";
        case BoxLineReduction:
            return "Box/Line Reduction";
        case NakedQuad:
            return "Naked Quad";
        case HiddenQuad:
            return "Hidden Quad";
        case XWing:
            return "X-Wing";
        case XYWing:
            return "XY-Wing";
        case Swordfish:
            return "Swordfish";
        case Skyscraper:
            return "Skyscraper";
        case TwoStringKite:
            return "2-String Kite";
        case XYZWing:
            return "XYZ-Wing";
        case UniqueRectangle:
            return "Unique Rectangle";
        case WWing:
            return "W-Wing";
        case SimpleColoring:
            return "Simple Coloring";
        case FinnedXWing:
            return "Finned X-Wing";
        case RemotePairs:
            return "Remote Pairs";
        case BUG:
            return "BUG";
        case Jellyfish:
            return "Jellyfish";
        case FinnedSwordfish:
            return "Finned Swordfish";
        case EmptyRectangle:
            return "Empty Rectangle";
        case WXYZWing:
            return "WXYZ-Wing";
        case FinnedJellyfish:
            return "Finned Jellyfish";
        case XYChain:
            return "XY-Chain";
        case MultiColoring:
            return "Multi-Coloring";
        case ALSxZ:
            return "ALS-XZ";
        case SueDeCoq:
            return "Sue de Coq";
        case ForcingChain:
            return "Forcing Chain";
        case NiceLoop:
            return "Nice Loop";
        case XCycles:
            return "X-Cycles";
        case ThreeDMedusa:
            return "3D Medusa";
        case HiddenUniqueRectangle:
            return "Hidden Unique Rectangle";
        case AvoidableRectangle:
            return "Avoidable Rectangle";
        case ALSXYWing:
            return "ALS-XY-Wing";
        case DeathBlossom:
            return "Death Blossom";
        case VWXYZWing:
            return "VWXYZ-Wing";
        case FrankenFish:
            return "Franken Fish";
        case GroupedXCycles:
            return "Grouped X-Cycles";
        case Backtracking:
            return "Backtracking";
    }
    return "Unknown Technique";
}

/// Returns Sudoku Explainer difficulty points for technique
/// @param technique The solving technique
/// @return Difficulty points (0 for Backtracking, >0 for logical techniques)
[[nodiscard]] constexpr int getTechniquePoints(SolvingTechnique technique) {
    using enum SolvingTechnique;
    switch (technique) {
        case NakedSingle:
            return 10;
        case HiddenSingle:
            return 15;
        case NakedPair:
            return 50;
        case NakedTriple:
            return 60;
        case HiddenPair:
            return 70;
        case HiddenTriple:
            return 90;
        case PointingPair:
        case BoxLineReduction:
            return 100;
        case NakedQuad:
            return 120;
        case HiddenQuad:
            return 150;
        case XWing:
        case XYWing:
            return 200;
        case Swordfish:
        case Skyscraper:
        case TwoStringKite:
            return 250;
        case XYZWing:
            return 280;
        case UniqueRectangle:
        case WWing:
            return 300;
        case SimpleColoring:
        case FinnedXWing:
        case RemotePairs:
        case BUG:
            return 350;
        case Jellyfish:
        case FinnedSwordfish:
        case EmptyRectangle:
        case WXYZWing:
        case MultiColoring:
            return 400;
        case FinnedJellyfish:
        case XYChain:
            return 450;
        case ALSxZ:
        case SueDeCoq:
            return 500;
        case ForcingChain:
            return 550;
        case NiceLoop:
            return 600;
        case XCycles:
            return 350;
        case ThreeDMedusa:
            return 400;
        case HiddenUniqueRectangle:
        case AvoidableRectangle:
            return 350;
        case ALSXYWing:
        case DeathBlossom:
            return 550;
        case VWXYZWing:
        case FrankenFish:
        case GroupedXCycles:
            return 450;
        case Backtracking:
            return 750;  // Trial-and-error: harder than all logical strategies
    }
    return 0;
}

/// Returns localized technique name using the localization manager.
/// @param loc Localization manager for string lookup
/// @param technique The solving technique
/// @return Localized technique name (e.g., "Naked Single" in English)
[[nodiscard]] inline const char* getLocalizedTechniqueName(const ILocalizationManager& loc,
                                                           SolvingTechnique technique) {
    using enum SolvingTechnique;
    using namespace StringKeys;
    switch (technique) {
        case NakedSingle:
            return loc.getString(TechNakedSingle);
        case HiddenSingle:
            return loc.getString(TechHiddenSingle);
        case NakedPair:
            return loc.getString(TechNakedPair);
        case NakedTriple:
            return loc.getString(TechNakedTriple);
        case HiddenPair:
            return loc.getString(TechHiddenPair);
        case HiddenTriple:
            return loc.getString(TechHiddenTriple);
        case PointingPair:
            return loc.getString(TechPointingPair);
        case BoxLineReduction:
            return loc.getString(TechBoxLineReduction);
        case NakedQuad:
            return loc.getString(TechNakedQuad);
        case HiddenQuad:
            return loc.getString(TechHiddenQuad);
        case XWing:
            return loc.getString(TechXWing);
        case XYWing:
            return loc.getString(TechXYWing);
        case Swordfish:
            return loc.getString(TechSwordfish);
        case Skyscraper:
            return loc.getString(TechSkyscraper);
        case TwoStringKite:
            return loc.getString(TechTwoStringKite);
        case XYZWing:
            return loc.getString(TechXYZWing);
        case UniqueRectangle:
            return loc.getString(TechUniqueRectangle);
        case WWing:
            return loc.getString(TechWWing);
        case SimpleColoring:
            return loc.getString(TechSimpleColoring);
        case FinnedXWing:
            return loc.getString(TechFinnedXWing);
        case RemotePairs:
            return loc.getString(TechRemotePairs);
        case BUG:
            return loc.getString(TechBUG);
        case Jellyfish:
            return loc.getString(TechJellyfish);
        case FinnedSwordfish:
            return loc.getString(TechFinnedSwordfish);
        case EmptyRectangle:
            return loc.getString(TechEmptyRectangle);
        case WXYZWing:
            return loc.getString(TechWXYZWing);
        case FinnedJellyfish:
            return loc.getString(TechFinnedJellyfish);
        case XYChain:
            return loc.getString(TechXYChain);
        case MultiColoring:
            return loc.getString(TechMultiColoring);
        case ALSxZ:
            return loc.getString(TechALSxZ);
        case SueDeCoq:
            return loc.getString(TechSueDeCoq);
        case ForcingChain:
            return loc.getString(TechForcingChain);
        case NiceLoop:
            return loc.getString(TechNiceLoop);
        case XCycles:
            return loc.getString(TechXCycles);
        case ThreeDMedusa:
            return loc.getString(TechThreeDMedusa);
        case HiddenUniqueRectangle:
            return loc.getString(TechHiddenUniqueRectangle);
        case AvoidableRectangle:
            return loc.getString(TechAvoidableRectangle);
        case ALSXYWing:
            return loc.getString(TechALSXYWing);
        case DeathBlossom:
            return loc.getString(TechDeathBlossom);
        case VWXYZWing:
            return loc.getString(TechVWXYZWing);
        case FrankenFish:
            return loc.getString(TechFrankenFish);
        case GroupedXCycles:
            return loc.getString(TechGroupedXCycles);
        case Backtracking:
            return loc.getString(TechBacktrackingName);
    }
    return loc.getString(TechUnknown);
}

}  // namespace sudoku::core
