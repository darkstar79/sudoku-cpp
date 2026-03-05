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

#include "main_window.h"

#include "../core/string_keys.h"
#include "../imgui_backends/imgui_impl_opengl3.h"
#include "../imgui_backends/imgui_impl_sdl3.h"
#include "core/constants.h"
#include "infrastructure/app_directory_manager.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

namespace sudoku::view {

using namespace core::StringKeys;

MainWindow::MainWindow()
    : font_manager_(std::make_unique<FontManager>()), last_auto_save_time_(std::chrono::steady_clock::now()) {
}

MainWindow::~MainWindow() {
    if (initialized_) {
        shutdown();
    }
}

bool MainWindow::initialize(const std::string& title, int width, int height) {
    spdlog::info("Initializing MainWindow: {}x{}", width, height);

    // Initialize SDL3
    if (!initializeSdl()) {
        return false;
    }

    // Setup OpenGL context and create window
    if (!setupOpenGlContext(title, width, height)) {
        return false;
    }

    // Log OpenGL capabilities
    logOpenGlInfo();

    // Initialize ImGui context and fonts
    if (!initializeImGui()) {
        return false;
    }

    // Determine GLSL version from OpenGL version
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* glsl_version = "#version 130";
    if (version && std::string(version).find("OpenGL ES") != std::string::npos) {
        glsl_version = "#version 300 es";
    } else if (version && std::string(version).find("2.") != std::string::npos) {
        glsl_version = "#version 120";
    }

    // Initialize ImGui rendering backends
    if (!initializeImGuiBackends(glsl_version)) {
        return false;
    }

    initialized_ = true;
    spdlog::info("MainWindow initialized successfully");
    return true;
}

void MainWindow::setLocalizationManager(std::shared_ptr<core::ILocalizationManager> loc_manager) {
    loc_manager_ = std::move(loc_manager);

    // Apply saved language preference (overrides default "en" from startup)
    auto saved_locale = loadLanguagePreference();
    if (!saved_locale.empty() && saved_locale != loc_manager_->getCurrentLocale()) {
        [[maybe_unused]] auto result = loc_manager_->setLocale(saved_locale);
    }

    spdlog::debug("LocalizationManager bound to MainWindow (locale: {})", loc_manager_->getCurrentLocale());
}

void MainWindow::saveLanguagePreference(std::string_view locale_code) {
    auto pref_dir =
        infrastructure::AppDirectoryManager::getDefaultDirectory(infrastructure::DirectoryType::Saves).parent_path();
    std::filesystem::create_directories(pref_dir);
    auto pref_file = pref_dir / "language.txt";
    std::ofstream out(pref_file);
    if (out) {
        out << locale_code;
    }
}

std::string MainWindow::loadLanguagePreference() {
    auto pref_file =
        infrastructure::AppDirectoryManager::getDefaultDirectory(infrastructure::DirectoryType::Saves).parent_path() /
        "language.txt";
    std::ifstream in(pref_file);
    std::string locale_code;
    if (in) {
        std::getline(in, locale_code);
    }
    return locale_code;
}

const char* MainWindow::difficultyString(core::Difficulty difficulty) const {
    switch (difficulty) {
        case core::Difficulty::Easy:
            return loc(DifficultyEasy);
        case core::Difficulty::Medium:
            return loc(DifficultyMedium);
        case core::Difficulty::Hard:
            return loc(DifficultyHard);
        case core::Difficulty::Expert:
            return loc(DifficultyExpert);
        case core::Difficulty::Master:
            return loc(DifficultyMaster);
        default:
            return loc(DifficultyUnknown);
    }
}

void MainWindow::setViewModel(std::shared_ptr<viewmodel::GameViewModel> view_model) {
    view_model_ = std::move(view_model);

    if (view_model_) {
        // Subscribe to view model changes
        observer_.observe(view_model_->gameState, [this](const auto&) { updateFromViewModel(); });
        observer_.observe(view_model_->uiState, [this](const auto&) { updateFromViewModel(); });
        observer_.observe(view_model_->errorMessage, [this](const std::string& error) {
            if (!error.empty()) {
                showError(error);
            }
        });

        spdlog::debug("ViewModel bound to MainWindow");
    }
}

void MainWindow::setTrainingViewModel(std::shared_ptr<viewmodel::TrainingViewModel> training_vm) {
    training_vm_ = std::move(training_vm);

    if (training_vm_) {
        observer_.observe(training_vm_->errorMessage, [this](const std::string& error) {
            if (!error.empty()) {
                showError(error);
            }
        });
        spdlog::debug("TrainingViewModel bound to MainWindow");
    }
}

void MainWindow::render() {
    if (!initialized_) {
        return;
    }

    // Check auto-save timer (only if game state is dirty)
    auto now = std::chrono::steady_clock::now();
    if (now - last_auto_save_time_ >= AUTO_SAVE_INTERVAL) {
        if (view_model_ && view_model_->isGameStateDirty()) {
            view_model_->autoSave();
            last_auto_save_time_ = now;
            spdlog::debug("Periodic auto-save triggered (30s interval)");
        } else if (view_model_) {
            spdlog::trace("Auto-save skipped - no changes since last save");
            last_auto_save_time_ = now;  // Reset timer anyway
        }
    }

    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Render UI components
    renderMenuBar();
    if (app_mode_ == AppMode::Play) {
        renderToolbar();
        renderGameBoard();
        renderStatusBar();
    } else {
        renderTrainingMode();
    }

    // Render dialogs
    if (show_new_game_dialog_) {
        renderNewGameDialog();
    }
    if (show_reset_dialog_) {
        renderResetDialog();
    }
    if (show_save_dialog_) {
        renderSaveDialog();
    }
    if (show_load_dialog_) {
        renderLoadDialog();
    }
    if (show_statistics_) {
        renderStatisticsDialog();
    }
    if (show_about_) {
        renderAboutDialog();
    }

    // Render toast notifications (always on top)
    toast_.render();

    // Rendering
    ImGui::Render();
    int width{0};
    int height{0};
    SDL_GetWindowSize(window_, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(UIConstants::Colors::BG_R, UIConstants::Colors::BG_G, UIConstants::Colors::BG_B,
                 1.00F);  // #FAF8F0 - Newspaper tint background
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window_);
}

void MainWindow::handleEvent(const SDL_Event& event) {
    // Process event through ImGui first
    ImGui_ImplSDL3_ProcessEvent(&event);

    switch (event.type) {
        case SDL_EVENT_QUIT:
            // Save game state before exiting
            if (view_model_) {
                spdlog::info("Application exit requested, saving game state...");
                view_model_->autoSave();
                spdlog::info("Game state saved, exiting application");
            }
            should_close_ = true;
            break;

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (event.window.windowID == SDL_GetWindowID(window_)) {
                // Save game state before closing
                if (view_model_) {
                    spdlog::info("Window close requested, saving game state...");
                    view_model_->autoSave();
                    spdlog::info("Game state saved, closing window");
                }
                should_close_ = true;
            }
            break;

        case SDL_EVENT_KEY_DOWN:
            handleKeyboardInput(event.key.scancode);
            break;

        default:
            break;
    }
}

void MainWindow::shutdown() {
    if (!initialized_) {
        return;
    }

    spdlog::info("Shutting down MainWindow");

    observer_.unsubscribeAll();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (gl_context_) {
        SDL_GL_DestroyContext(gl_context_);
        gl_context_ = nullptr;
    }

    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
    initialized_ = false;

    spdlog::info("MainWindow shutdown complete");
}

void MainWindow::updateFromViewModel() {
    // This method would update UI state based on view model changes
    // For now, the UI updates automatically through data binding
}

void MainWindow::showError(std::string_view message) {
    spdlog::error("UI Error: {}", message);
    // For now, just log errors. In a full implementation, this would show an error dialog
}

void MainWindow::showStatus(std::string_view message) {
    spdlog::info("Status: {}", message);
}

void MainWindow::setupImGuiStyle() {
    ImGui::StyleColorsLight();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = 4.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.WindowPadding = ImVec2(0, 0);
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 4);

    // Newspaper-like color palette from mockup
    ImVec4* colors = style.Colors;

    // Window backgrounds - newspaper tint
    colors[ImGuiCol_WindowBg] =
        ImVec4(UIConstants::Colors::BG_R, UIConstants::Colors::BG_G, UIConstants::Colors::BG_B, 1.00F);  // #FAF8F0
    colors[ImGuiCol_ChildBg] =
        ImVec4(UIConstants::Colors::BG_R, UIConstants::Colors::BG_G, UIConstants::Colors::BG_B, 1.00F);
    colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);

    // Text colors - soft black
    colors[ImGuiCol_Text] = ImVec4(0.078f, 0.078f, 0.078f, 1.00f);       // #141414
    colors[ImGuiCol_TextDisabled] = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);  // #6B6B6B

    // Borders and separators
    colors[ImGuiCol_Border] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);  // #8C8C8C
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);

    // Headers and menus
    colors[ImGuiCol_Header] = ImVec4(UIConstants::Colors::LIGHT_GRAY_R, UIConstants::Colors::LIGHT_GRAY_G,
                                     UIConstants::Colors::LIGHT_GRAY_B, 1.00F);  // #f8f8f8
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.878f, 0.878f, 0.878f, 1.00f);      // #e0e0e0
    colors[ImGuiCol_HeaderActive] = ImVec4(0.804f, 0.804f, 0.804f, 1.00f);

    // Buttons
    colors[ImGuiCol_Button] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.961f, 0.961f, 0.961f, 1.00f);  // #f5f5f5
    colors[ImGuiCol_ButtonActive] = ImVec4(0.878f, 0.878f, 0.878f, 1.00f);

    // Menu bar
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.973f, 0.973f, 0.973f, 1.00f);  // #f8f8f8

    // Frame backgrounds
    colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.961f, 0.961f, 0.961f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.922f, 0.922f, 0.922f, 1.00f);

    // Tabs
    colors[ImGuiCol_Tab] = ImVec4(0.973f, 0.973f, 0.973f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.878f, 0.878f, 0.878f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);

    // Title bar
    colors[ImGuiCol_TitleBg] = ImVec4(0.961f, 0.961f, 0.961f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.973f, 0.973f, 0.973f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.973f, 0.973f, 0.973f, 0.50f);

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.98f, 0.973f, 0.941f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

    // Selection colors
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.996f, 0.953f, 0.780f, 0.35f);  // #FEF3C7 with transparency
}

// ============================================================================
// Initialization Helper Methods
// ============================================================================

bool MainWindow::initializeSdl() {
    // Try to force X11 video driver if available (helps with EGL issues)
    constexpr std::array<const char*, 2> preferred_drivers = {"x11", "wayland"};
    for (const char* driver : preferred_drivers) {
        if (SDL_SetHint(SDL_HINT_VIDEO_DRIVER, driver)) {
            spdlog::info("Trying video driver: {}", driver);
            if (SDL_Init(SDL_INIT_VIDEO)) {
                spdlog::info("Successfully initialized SDL3 with {} driver", driver);

                // Log available video drivers
                int num_drivers = SDL_GetNumVideoDrivers();
                spdlog::info("Available video drivers: {}", num_drivers);
                for (int i = 0; i < num_drivers; ++i) {
                    spdlog::info("  Driver {}: {}", i, SDL_GetVideoDriver(i));
                }
                spdlog::info("Current video driver: {}",
                             SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "none");

                return true;
            }
            spdlog::warn("Failed to initialize SDL3 with {} driver: {}", driver, SDL_GetError());
            SDL_Quit();  // Clean up before trying next driver
        }
    }

    // If none of the preferred drivers worked, try default
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, nullptr);  // Reset to default
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        spdlog::error("Failed to initialize SDL3 with default driver: {}", SDL_GetError());
        return false;
    }
    spdlog::info("Successfully initialized SDL3 with default driver");
    return true;
}

bool MainWindow::setupOpenGlContext(const std::string& title, int width, int height) {
    // Try different OpenGL configurations

    // First try: OpenGL 3.3 Core
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // Create window
    window_ = SDL_CreateWindow(title.c_str(), width, height,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

    if (!window_) {
        spdlog::warn("Failed to create window with OpenGL 3.3 Core: {}", SDL_GetError());

        // Fallback: Try OpenGL 3.0
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

        window_ = SDL_CreateWindow(title.c_str(), width, height,
                                   SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

        if (!window_) {
            spdlog::warn("Failed to create window with OpenGL 3.0: {}", SDL_GetError());

            // Final fallback: Try compatibility profile
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

            window_ = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

            if (!window_) {
                spdlog::error("Failed to create SDL3 window with all fallbacks: {}", SDL_GetError());
                return false;
            }
            spdlog::info("Successfully created window with OpenGL 2.1 compatibility");
        } else {
            spdlog::info("Successfully created window with OpenGL 3.0");
        }
    } else {
        spdlog::info("Successfully created window with OpenGL 3.3 Core");
    }

    // Create OpenGL context
    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
        spdlog::error("Failed to create OpenGL context: {}", SDL_GetError());
        return false;
    }

    SDL_GL_MakeCurrent(window_, gl_context_);

    // Set swap interval (vsync) - may fail, so don't error out
    if (!SDL_GL_SetSwapInterval(1)) {
        spdlog::warn("Failed to enable vsync: {}", SDL_GetError());
    }

    return true;
}

void MainWindow::logOpenGlInfo() {
    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));

    spdlog::info("OpenGL Vendor: {}", vendor ? vendor : "unknown");
    spdlog::info("OpenGL Renderer: {}", renderer ? renderer : "unknown");
    spdlog::info("OpenGL Version: {}", version ? version : "unknown");
}

bool MainWindow::initializeImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Initialize font manager
    if (!font_manager_->initialize()) {
        spdlog::error("Failed to initialize FontManager");
        return false;
    }

    setupImGuiStyle();
    return true;
}

bool MainWindow::initializeImGuiBackends(const char* gl_version) {
    if (!ImGui_ImplSDL3_InitForOpenGL(window_, gl_context_)) {
        spdlog::error("Failed to initialize ImGui SDL3 backend");
        return false;
    }

    spdlog::info("Using GLSL version: {}", gl_version);

    if (!ImGui_ImplOpenGL3_Init(gl_version)) {
        spdlog::error("Failed to initialize ImGui OpenGL3 backend");
        return false;
    }

    return true;
}
}  // namespace sudoku::view