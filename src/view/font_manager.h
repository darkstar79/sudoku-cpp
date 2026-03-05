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

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>

namespace sudoku::view {

/// Font types used in the Sudoku application
enum class FontType : std::uint8_t {
    Normal,  // UI text (14px)
    Large,   // Main Sudoku numbers (40px)
    Medium   // Note numbers (20px)
};

/// Manages font loading, configuration, and access for the Sudoku application
class FontManager {
public:
    FontManager() = default;
    ~FontManager() = default;

    // Non-copyable
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;
    FontManager(FontManager&&) = delete;
    FontManager& operator=(FontManager&&) = delete;

    /// Initialize fonts with ImGui
    /// @return true if successful, false otherwise
    [[nodiscard]] bool initialize();

    /// Get font for specific type
    /// @param type Font type to retrieve
    /// @return ImGui font pointer, guaranteed non-null (falls back to default)
    /// @note Returned pointer is owned by ImGui font atlas, do not delete
    [[nodiscard]] ImFont* getFont(FontType type) const noexcept;

    /// Check if high-quality vector fonts are loaded
    /// @return true if using TTF fonts, false if using bitmap fallback
    [[nodiscard]] bool hasVectorFonts() const {
        return has_vector_fonts_;
    }

    /// Get font size for a specific type
    /// @param type Font type
    /// @return Size in pixels
    [[nodiscard]] static float getFontSize(FontType type) noexcept;

    /// Check if a specific font type is available and loaded
    /// @param type Font type to check
    /// @return true if font is loaded and available
    [[nodiscard]] bool isFontLoaded(FontType type) const noexcept;

private:
    /// Font configuration for different types
    struct FontConfig {
        float size_px;
        int oversample_h;
        int oversample_v;
        float rasterizer_multiply;
        bool pixel_snap_h;
    };

    /// Try to load a TTF font from system paths
    /// @param size Font size in pixels
    /// @param config Font configuration
    /// @return ImFont pointer or nullptr if failed (owned by ImGui)
    [[nodiscard]] ImFont* loadSystemFont(float size, const FontConfig& config) noexcept;

    /// Create fallback font using ImGui default
    /// @param size Font size in pixels
    /// @param config Font configuration
    /// @return ImFont pointer, guaranteed non-null (owned by ImGui)
    [[nodiscard]] static ImFont* createFallbackFont(float size, const FontConfig& config) noexcept;

    /// Get system font paths to try
    /// @return Vector of font file paths
    [[nodiscard]] static std::vector<std::string> getSystemFontPaths();

    /// Configure font atlas for optimal quality
    static void configureFontAtlas();

    // Font configurations
    static constexpr FontConfig NORMAL_CONFIG = {
        .size_px = 14.0f, .oversample_h = 2, .oversample_v = 2, .rasterizer_multiply = 1.0f, .pixel_snap_h = true};
    static constexpr FontConfig LARGE_CONFIG = {
        .size_px = 40.0f, .oversample_h = 8, .oversample_v = 8, .rasterizer_multiply = 1.3f, .pixel_snap_h = true};
    static constexpr FontConfig MEDIUM_CONFIG = {
        .size_px = 20.0f, .oversample_h = 6, .oversample_v = 6, .rasterizer_multiply = 1.3f, .pixel_snap_h = true};

    // Font storage - owned by ImGui font atlas, not by us
    // These are non-owning pointers that reference ImGui-managed fonts
    ImFont* normal_font_{nullptr};  ///< Normal UI font (14px)
    ImFont* large_font_{nullptr};   ///< Large numbers font (40px)
    ImFont* medium_font_{nullptr};  ///< Medium notes font (20px)

    // State
    bool initialized_{false};
    bool has_vector_fonts_{false};
    std::string loaded_font_path_;
};

}  // namespace sudoku::view