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

#include "font_manager.h"

#include <filesystem>

#include <spdlog/spdlog.h>

namespace sudoku::view {

bool FontManager::initialize() {
    if (initialized_) {
        spdlog::warn("FontManager already initialized");
        return true;
    }

    ImGuiIO& io = ImGui::GetIO();

    // Clear existing fonts to start fresh
    io.Fonts->Clear();

    // Configure font atlas for optimal quality
    configureFontAtlas();

    // Load normal font (always use default for UI consistency)
    io.Fonts->AddFontDefault();
    normal_font_ = io.Fonts->Fonts[io.Fonts->Fonts.Size - 1];

    // Try to load vector fonts for large display
    large_font_ = loadSystemFont(LARGE_CONFIG.size_px, LARGE_CONFIG);
    if (large_font_) {
        has_vector_fonts_ = true;
        spdlog::info("Loaded large vector font ({}px): {}", LARGE_CONFIG.size_px, loaded_font_path_);

        // Try to load medium font from same source
        medium_font_ = loadSystemFont(MEDIUM_CONFIG.size_px, MEDIUM_CONFIG);
        if (medium_font_) {
            spdlog::info("Loaded medium vector font ({}px): {}", MEDIUM_CONFIG.size_px, loaded_font_path_);
        }
    }

    // Create fallbacks for any missing fonts
    if (!large_font_) {
        large_font_ = createFallbackFont(LARGE_CONFIG.size_px, LARGE_CONFIG);
        spdlog::warn("Using bitmap fallback for large font - quality will be poor");
    }

    if (!medium_font_) {
        medium_font_ = createFallbackFont(MEDIUM_CONFIG.size_px, MEDIUM_CONFIG);
        if (!has_vector_fonts_) {
            spdlog::warn("Using bitmap fallback for medium font - quality will be poor");
        }
    }

    // Build font atlas
    if (!io.Fonts->Build()) {
        spdlog::error("Failed to build font atlas");
        return false;
    }

    initialized_ = true;

    spdlog::info("FontManager initialized - Vector fonts: {}, Normal: {}px, Large: {}px, Medium: {}px",
                 has_vector_fonts_ ? "YES" : "NO", NORMAL_CONFIG.size_px, LARGE_CONFIG.size_px, MEDIUM_CONFIG.size_px);

    return true;
}

ImFont* FontManager::getFont(FontType type) const noexcept {
    if (!initialized_) {
        spdlog::error("FontManager not initialized, returning ImGui default font");
        // Always return a valid font - ImGui guarantees default font exists
        ImGuiIO& io = ImGui::GetIO();
        return io.Fonts->Fonts.empty() ? nullptr : io.Fonts->Fonts[0];
    }

    ImFont* selected_font = nullptr;
    switch (type) {
        case FontType::Normal:
            selected_font = normal_font_;
            break;
        case FontType::Large:
            selected_font = large_font_;
            break;
        case FontType::Medium:
            selected_font = medium_font_;
            break;
        default:
            spdlog::warn("Unknown FontType requested, falling back to Normal");
            selected_font = normal_font_;
            break;
    }

    // Ensure we never return null - fallback to ImGui default if needed
    if (!selected_font) {
        spdlog::warn("Requested font is null, falling back to ImGui default");
        ImGuiIO& io = ImGui::GetIO();
        return io.Fonts->Fonts.empty() ? nullptr : io.Fonts->Fonts[0];
    }

    return selected_font;
}

float FontManager::getFontSize(FontType type) noexcept {
    switch (type) {
        case FontType::Normal:
            return NORMAL_CONFIG.size_px;
        case FontType::Large:
            return LARGE_CONFIG.size_px;
        case FontType::Medium:
            return MEDIUM_CONFIG.size_px;
        default:
            spdlog::warn("Unknown FontType in getFontSize, returning Normal size");
            return NORMAL_CONFIG.size_px;
    }
}

bool FontManager::isFontLoaded(FontType type) const noexcept {
    if (!initialized_) {
        return false;
    }

    switch (type) {
        case FontType::Normal:
            return normal_font_ != nullptr;
        case FontType::Large:
            return large_font_ != nullptr;
        case FontType::Medium:
            return medium_font_ != nullptr;
        default:
            spdlog::warn("Unknown FontType in isFontLoaded");
            return false;
    }
}

ImFont* FontManager::loadSystemFont(float size, const FontConfig& config) noexcept {
    try {
        ImGuiIO& io = ImGui::GetIO();

        // Configure ImGui font config
        ImFontConfig img_config;
        img_config.SizePixels = size;
        img_config.OversampleH = static_cast<signed char>(config.oversample_h);
        img_config.OversampleV = static_cast<signed char>(config.oversample_v);
        img_config.RasterizerMultiply = config.rasterizer_multiply;
        img_config.PixelSnapH = config.pixel_snap_h;

        // Try each font path
        for (const auto& path : getSystemFontPaths()) {
            if (std::filesystem::exists(path)) {
                ImFont* font = io.Fonts->AddFontFromFileTTF(path.c_str(), size, &img_config);
                if (font) {
                    loaded_font_path_ = path;
                    return font;
                }
            }
        }

        return nullptr;
    } catch (const std::exception& e) {
        spdlog::error("Exception while loading system font: {}", e.what());
        return nullptr;
    } catch (...) {
        spdlog::error("Unknown exception while loading system font");
        return nullptr;
    }
}

ImFont* FontManager::createFallbackFont(float size, const FontConfig& config) noexcept {
    try {
        ImGuiIO& io = ImGui::GetIO();

        // Configure ImGui font config for scaled default font
        ImFontConfig img_config;
        img_config.SizePixels = size;
        img_config.OversampleH = static_cast<signed char>(config.oversample_h);
        img_config.OversampleV = static_cast<signed char>(config.oversample_v);
        img_config.RasterizerMultiply = config.rasterizer_multiply;
        img_config.PixelSnapH = config.pixel_snap_h;

        ImFont* font = io.Fonts->AddFontDefault(&img_config);
        if (!font) {
            spdlog::error("CRITICAL: ImGui AddFontDefault returned null - this should never happen");
        }
        return font;  // AddFontDefault should never return null
    } catch (const std::exception& e) {
        spdlog::error("Exception while creating fallback font: {}", e.what());
        return nullptr;
    } catch (...) {
        spdlog::error("Unknown exception while creating fallback font");
        return nullptr;
    }
}

std::vector<std::string> FontManager::getSystemFontPaths() {
    return {// Google fonts (priority - good quality)
            "/usr/share/fonts/google-carlito-fonts/Carlito-Bold.ttf",
            "/usr/share/fonts/google-carlito-fonts/Carlito-Regular.ttf",
            "/usr/share/fonts/google-crosextra-caladea-fonts/Caladea-Bold.ttf",
            "/usr/share/fonts/google-crosextra-caladea-fonts/Caladea-Regular.ttf",

            // Standard Linux fonts
            "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf", "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",

            // macOS fonts
            "/System/Library/Fonts/Helvetica.ttc", "/System/Library/Fonts/Arial.ttf",

            // Windows fonts
            "/Windows/Fonts/arial.ttf", "/Windows/Fonts/calibri.ttf"};
}

void FontManager::configureFontAtlas() {
    ImGuiIO& io = ImGui::GetIO();

    // Configure atlas for high quality
    io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;

    // Additional quality settings could go here
}

}  // namespace sudoku::view