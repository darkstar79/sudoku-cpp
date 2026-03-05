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

#include "../core/i_localization_manager.h"
#include "../core/observable.h"
#include "../core/training_types.h"
#include "../view_model/game_view_model.h"
#include "../view_model/training_view_model.h"
#include "font_manager.h"
#include "toast_notification.h"

#include <chrono>
#include <memory>
#include <string>

#include <SDL3/SDL.h>
#include <fmt/format.h>
#include <imgui.h>

// Forward declarations for ImGui types
struct ImDrawList;
struct ImVec2;

namespace sudoku::view {

// UI Constants
namespace UIConstants {
// Board sizing
inline constexpr float BOARD_MAX_SIZE = 720.0F;
inline constexpr float BOARD_MIN_SIZE = 450.0F;

// Newspaper theme colors (as ImVec4 components)
namespace Colors {
// Background: #FAF8F0
inline constexpr float BG_R = 0.98F;
inline constexpr float BG_G = 0.973F;
inline constexpr float BG_B = 0.941F;

// Light gray: #F8F8F8
inline constexpr float LIGHT_GRAY_R = 0.973F;
inline constexpr float LIGHT_GRAY_G = 0.973F;
inline constexpr float LIGHT_GRAY_B = 0.973F;

// Medium gray: #F0F0F0
inline constexpr float MEDIUM_GRAY_R = 0.941F;
inline constexpr float MEDIUM_GRAY_G = 0.941F;
inline constexpr float MEDIUM_GRAY_B = 0.941F;

// Board colors (IM_COL32 format: R, G, B, A)
// Border and grid colors
inline constexpr unsigned int BOARD_BORDER = 0xFF2C2C2C;      // #2C2C2C (44, 44, 44)
inline constexpr unsigned int BOARD_BACKGROUND = 0xFFFFFFFF;  // White
inline constexpr unsigned int GRID_THICK_LINE = 0xFF2C2C2C;   // #2C2C2C (44, 44, 44)
inline constexpr unsigned int GRID_THIN_LINE = 0xFF8C8C8C;    // #8C8C8C (140, 140, 140)

// Cell colors
inline constexpr unsigned int CELL_BACKGROUND = 0xFFFFFFFF;         // White
inline constexpr unsigned int CELL_SELECTED = 0xFFC7F3FE;           // #FEF3C7 (254, 243, 199) - yellow
inline constexpr unsigned int CELL_SELECTION_OUTLINE = 0xFF141414;  // #141414 (20, 20, 20)

// Text colors
inline constexpr unsigned int TEXT_GIVEN = 0xFF141414;  // #141414 (20, 20, 20) - given numbers
inline constexpr unsigned int TEXT_USER = 0xFFCC5200;   // #0052CC (0, 82, 204) - user numbers (blue)
inline constexpr unsigned int TEXT_ERROR = 0xFF2626DC;  // #DC2626 (220, 38, 38) - errors (red)
inline constexpr unsigned int TEXT_HINT = 0xFF00A5FF;   // #FFA500 (255, 165, 0) - hint-revealed (orange)
inline constexpr unsigned int TEXT_NOTE = 0xFF6B6B6B;   // #6B6B6B (107, 107, 107) - notes (gray)

// Training mode cell highlight colors (IM_COL32: ABGR)
inline constexpr unsigned int TRAIN_PATTERN = 0xFFE0FFE0;    // Light green
inline constexpr unsigned int TRAIN_ELIMINATE = 0xFFE0E0FF;  // Light red
inline constexpr unsigned int TRAIN_REGION = 0xFFFFF0E0;     // Light blue
inline constexpr unsigned int TRAIN_CHAIN_A = 0xFFF0F0D0;    // Light cyan
inline constexpr unsigned int TRAIN_CHAIN_B = 0xFFC0D0FF;    // Light orange
}  // namespace Colors

// Buffer sizes
inline constexpr size_t SAVE_NAME_BUFFER_SIZE = 256;
}  // namespace UIConstants

/// Main application window using Dear ImGui
class MainWindow {
public:
    /// Constructor
    MainWindow();

    /// Destructor
    ~MainWindow();
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;
    MainWindow(MainWindow&&) = delete;
    MainWindow& operator=(MainWindow&&) = delete;

    /// Initialize the window and rendering context
    bool initialize(const std::string& title = "Sudoku", int width = 800, int height = 900);

    /// Set the view model for data binding
    void setViewModel(std::shared_ptr<viewmodel::GameViewModel> view_model);

    /// Set the training view model for Training Mode
    void setTrainingViewModel(std::shared_ptr<viewmodel::TrainingViewModel> training_vm);

    /// Set the localization manager for UI string lookups
    void setLocalizationManager(std::shared_ptr<core::ILocalizationManager> loc_manager);

    /// Main render loop (call this every frame)
    void render();

    /// Handle SDL events
    void handleEvent(const SDL_Event& event);

    /// Check if window should close
    [[nodiscard]] bool shouldClose() const {
        return should_close_;
    }

    /// Shutdown and cleanup
    void shutdown();

private:
    // SDL/OpenGL resources
    SDL_Window* window_{nullptr};
    SDL_GLContext gl_context_{nullptr};
    bool should_close_{false};
    bool initialized_{false};

    /// Application mode: Play (normal game) or Training
    enum class AppMode : std::uint8_t {
        Play,
        Training
    };
    AppMode app_mode_{AppMode::Play};

    // ViewModel binding
    std::shared_ptr<viewmodel::GameViewModel> view_model_;
    std::shared_ptr<viewmodel::TrainingViewModel> training_vm_;
    core::CompositeObserver observer_;

    // UI state
    bool show_statistics_{false};
    bool show_about_{false};
    bool show_new_game_dialog_{false};
    bool show_reset_dialog_{false};
    bool show_save_dialog_{false};
    bool show_load_dialog_{false};
    std::string save_name_buffer_;
    int selected_difficulty_{1};  // Medium (UI index: 0=Easy, 1=Medium, 2=Hard, 3=Expert)
    bool menu_visible_{true};     // Menu visibility (start visible, toggle with F1)

    // Localization
    std::shared_ptr<core::ILocalizationManager> loc_manager_;
    int selected_language_{0};  // Index into available locales list

    /// Get a localized string by key (convenience wrapper)
    [[nodiscard]] const char* loc(std::string_view key) const {
        return loc_manager_ ? loc_manager_->getString(key) : key.data();
    }

    /// Format a localized string with arguments (fmt-style positional placeholders)
    template <typename... Args>
    [[nodiscard]] std::string locFormat(std::string_view key, Args&&... args) const {
        return fmt::format(fmt::runtime(loc(key)), std::forward<Args>(args)...);
    }

    // Language preference persistence
    static void saveLanguagePreference(std::string_view locale_code);
    [[nodiscard]] static std::string loadLanguagePreference();

    // Font management
    std::unique_ptr<FontManager> font_manager_;

    // Toast notifications
    ToastNotification toast_;

    // Double-press detection for final number entry
    int last_number_pressed_{0};
    std::chrono::steady_clock::time_point last_press_time_;
    static constexpr std::chrono::milliseconds double_press_threshold_{300};

    // Auto-save timer
    std::chrono::steady_clock::time_point last_auto_save_time_;
    static constexpr std::chrono::seconds AUTO_SAVE_INTERVAL{30};

    /// Get localized difficulty name from Difficulty enum
    [[nodiscard]] const char* difficultyString(core::Difficulty difficulty) const;

    // Rendering methods
    void renderMenuBar();
    void renderToolbar();
    void renderGameBoard();
    void renderStatusBar();
    void renderSidebar();

    // Dialog rendering
    void renderNewGameDialog();
    void renderResetDialog();
    void renderSaveDialog();
    void renderLoadDialog();
    void renderStatisticsDialog();
    void renderAboutDialog();

    // CSV Export handlers
    void exportAggregateStatsCsv();
    void exportGameSessionsCsv();

    // Training Mode rendering
    void renderTrainingMode();
    void renderTechniqueSelection();
    void renderTheoryReview();
    void renderTrainingExercise();
    void renderTrainingFeedback();
    void renderLessonComplete();
    static void renderTrainingBoard(const core::TrainingBoard& board, float board_size);
    [[nodiscard]] static unsigned int getCellRoleColor(core::CellRole role);

    // Board rendering helpers
    static void renderBoardBackground(ImDrawList* draw_list, const ImVec2& canvas_pos, float board_size);
    void renderSingleCell(ImDrawList* draw_list, const model::Cell& cell, size_t row, size_t col,
                          const ImVec2& cell_pos, float cell_size, bool is_selected);
    void renderCellValue(ImDrawList* draw_list, const model::Cell& cell, const ImVec2& cell_pos, float cell_size);
    void renderCellNotes(ImDrawList* draw_list, const model::Cell& cell, const ImVec2& cell_pos, float cell_size);
    static void renderBoardGridLines(ImDrawList* draw_list, const ImVec2& canvas_pos, float board_size,
                                     float cell_size);
    void renderActionButtons(const ImVec2& board_pos, float board_size);

    // Input handling
    void handleCellClick(int row, int col);
    void handleKeyboardInput(int scancode);
    void handleNumberInput(int number);

    // Keyboard input helpers
    void handleNavigationInput(int scancode, const core::Position& current_pos);
    void handleModifiedKeyInput(int scancode);
    void handleEditingInput(int scancode);
    void handleFunctionKeyInput(int scancode);

    // UI helpers
    void updateFromViewModel();
    static void showError(std::string_view message);
    static void showStatus(std::string_view message);
    void renderRatingTooltip(const viewmodel::UIState& ui_state);

    // Initialization helpers
    static bool initializeSdl();
    bool setupOpenGlContext(const std::string& title, int width, int height);
    static void logOpenGlInfo();
    bool initializeImGui();
    bool initializeImGuiBackends(const char* gl_version);

    // Styling
    static void setupImGuiStyle();

    // Constants
    static constexpr float CELL_SIZE = 45.0f;
    static constexpr float GRID_PADDING = 10.0f;
    static constexpr float THICK_LINE_WIDTH = 3.0f;
    static constexpr float THIN_LINE_WIDTH = 1.0f;
};

}  // namespace sudoku::view