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

#pragma once

#include <cstdint>
#include <vector>

#include <QString>
#include <QtGlobal>
#include <qnamespace.h>

namespace sudoku::view {

/// Semantic category of a keyboard shortcut row. Stable, language-neutral identifier:
/// tests assert on the category (not the localized label), and the human label/chord are
/// derived from it. The categories mirror the binding set implemented in story 0a.2.
enum class ShortcutAction : std::uint8_t {
    ActiveModeDigit,   ///< plain digit — acts on the active input mode's layer
    ValueOverride,     ///< Ctrl/Cmd + digit — force a final value
    PencilOverride,    ///< Shift + digit — toggle a pencil mark
    ColorOverride,     ///< Alt/Option + digit — apply a color (1..palette)
    CycleMode,         ///< Space — cycle Normal → Notes → Color
    ClearActiveLayer,  ///< Delete / Backspace / 0 — clear the active layer
    ClearValue,        ///< Ctrl + 0/Delete — clear the value
    ClearColor,        ///< Alt + 0/Delete — clear the color
    ClearPencilMarks,  ///< Shift + Delete — clear all pencil marks
    Pause,             ///< P — pause/resume: stop the timer and hide the board (story 6.8)
};

/// One row of the keyboard map. This struct, returned by keyboardShortcuts(), is the
/// **single source of truth** for the surfaced shortcuts — both the Keyboard Shortcuts
/// dialog and the status-line hint render from it, so the documented map cannot drift
/// from the actual bindings (story 0a.3, AC-5).
struct ShortcutEntry {
    ShortcutAction action{};
    Qt::KeyboardModifiers modifiers;  ///< modifier portion of the chord (NoModifier for plain keys)
    int key{};                        ///< representative Qt::Key for the non-modifier part
    bool digit_family{};              ///< true → `key` is a placeholder for a 1..digit_max range
    int digit_max{};                  ///< upper bound of the digit range (only meaningful when digit_family)
};

/// The canonical keyboard map, in display order. The one list the dialog and hint consume.
/// coloring_enabled=false (story 8-21) omits the ColorOverride and ClearColor rows — the color
/// layer entry points that story gates. Defaulted true so every existing call site and test
/// keeps asserting today's (coloring-enabled) behavior unchanged.
[[nodiscard]] std::vector<ShortcutEntry> keyboardShortcuts(bool coloring_enabled = true);

/// Localized human label for an action category (via core::loc). With coloring_enabled=false,
/// CycleMode returns the Normal -> Notes wording (story 8-21); every other action is unaffected.
[[nodiscard]] QString shortcutActionLabel(ShortcutAction action, bool coloring_enabled = true);

/// Native modifier name only — "Ctrl"/"Alt"/"Shift" on Windows/Linux, "⌘/⌥/⇧" on macOS —
/// rendered via QKeySequence::toString(NativeText). Empty for Qt::NoModifier.
[[nodiscard]] QString nativeModifierName(Qt::KeyboardModifiers modifiers);

/// Full native chord text for an entry: NativeText modifier glyph(s) joined with the key
/// (or a "1–N" range for digit families). e.g. "Ctrl+1–9", "⌥1–6", "Space", "Shift+Del".
[[nodiscard]] QString shortcutChordText(const ShortcutEntry& entry);

/// Status-line micro-hint, e.g. "Ctrl=value · Shift=pencil · Alt=color": modifier names via
/// NativeText (per OS), action words via core::loc. With coloring_enabled=false (story 8-21) the
/// "Alt=color" segment and its separator are dropped.
[[nodiscard]] QString modifierHintText(bool coloring_enabled = true);

}  // namespace sudoku::view
