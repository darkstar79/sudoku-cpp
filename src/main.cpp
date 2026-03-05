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

#include "imgui_backends/imgui_impl_opengl3.h"
#include "imgui_backends/imgui_impl_sdl3.h"

#include <filesystem>
#include <memory>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

// New architecture includes
#include "core/di_container.h"
#include "view/main_window.h"
#include "view_model/game_view_model.h"

// Real implementations
#include "core/game_validator.h"
#include "core/i_localization_manager.h"
#include "core/i_time_provider.h"
#include "core/localization_manager.h"
#include "core/puzzle_generator.h"
#include "core/puzzle_rater.h"
#include "core/save_manager.h"
#include "core/statistics_manager.h"
#include "core/sudoku_solver.h"
#include "core/training_exercise_generator.h"
#include "view_model/training_view_model.h"

namespace sudoku {
// Mock implementations removed - using real implementations

// All mock implementations removed - using real implementations

}  // namespace sudoku

class Application {
public:
    Application() = default;
    ~Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    bool Initialize() {
        spdlog::set_level(spdlog::level::debug);
        spdlog::info("Initializing Sudoku application with new MVVM architecture");

        // Register dependencies in DI container
        setupDependencies();

        // Initialize main window
        main_window_ = std::make_unique<sudoku::view::MainWindow>();
        if (!main_window_->initialize()) {
            spdlog::error("Failed to initialize main window");
            return false;
        }

        // Create and set view model
        auto view_model = createViewModel();
        main_window_->setViewModel(view_model);

        // Set localization manager
        auto loc_manager = sudoku::core::getContainer().resolve<sudoku::core::ILocalizationManager>();
        main_window_->setLocalizationManager(loc_manager);

        // Create and set training view model
        auto exercise_gen = sudoku::core::getContainer().resolve<sudoku::core::ITrainingExerciseGenerator>();
        auto training_vm = std::make_shared<sudoku::viewmodel::TrainingViewModel>(exercise_gen, loc_manager);
        main_window_->setTrainingViewModel(training_vm);

        // Try to load auto-save, otherwise start new game
        auto& container = sudoku::core::getContainer();
        auto save_manager =
            std::static_pointer_cast<sudoku::core::ISaveManager>(container.resolve<sudoku::core::ISaveManager>());

        if (save_manager->hasAutoSave()) {
            spdlog::info("Auto-save detected, attempting to load...");
            auto result = save_manager->loadAutoSave();

            if (result.has_value()) {
                // Check if saved game is completed - don't restore completed games
                if (result->is_complete) {
                    spdlog::info("Auto-save contains completed game, starting fresh");
                    view_model->startNewGame(sudoku::core::Difficulty::Medium);
                } else {
                    // Restore in-progress game state from auto-save
                    view_model->restoreGameState(result.value());
                    spdlog::info("Auto-save loaded successfully");
                }
            } else {
                spdlog::warn("Auto-save load failed: {}, starting new game", static_cast<int>(result.error()));
                view_model->startNewGame(sudoku::core::Difficulty::Medium);
            }
        } else {
            spdlog::info("No auto-save found, starting new Medium game");
            view_model->startNewGame(sudoku::core::Difficulty::Medium);
        }

        spdlog::info("Application initialized successfully with MVVM architecture!");
        return true;
    }

    void Run() {
        spdlog::info("Running application main loop...");

        bool was_suspended = false;

        while (!main_window_->shouldClose()) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                // Detect system suspend/resume events
                if (event.type == SDL_EVENT_WINDOW_MINIMIZED) {
                    was_suspended = true;
                    spdlog::debug("Window minimized/suspended");
                } else if (event.type == SDL_EVENT_WINDOW_RESTORED && was_suspended) {
                    // Clear accumulated mouse events after resume
                    int flushed = 0;
                    SDL_Event temp_event;
                    while (SDL_PeepEvents(&temp_event, 1, SDL_GETEVENT, SDL_EVENT_MOUSE_MOTION,
                                          SDL_EVENT_MOUSE_MOTION) > 0) {
                        flushed++;
                    }
                    if (flushed > 0) {
                        spdlog::debug("Cleared {} accumulated mouse motion events after resume", flushed);
                    }
                    was_suspended = false;
                }

                main_window_->handleEvent(event);
            }

            main_window_->render();
        }

        spdlog::info("Application main loop completed");
    }

    void Shutdown() {
        spdlog::info("Shutting down application...");

        if (main_window_) {
            main_window_->shutdown();
            main_window_.reset();
        }

        // Clear DI container
        sudoku::core::getContainer().clear();

        spdlog::info("Application shutdown complete");
    }

private:
    std::unique_ptr<sudoku::view::MainWindow> main_window_;

    static void setupDependencies() {
        auto& container = sudoku::core::getContainer();

        // Register time provider (foundation dependency)
        container.registerSingleton<sudoku::core::ITimeProvider>(
            []() { return std::make_unique<sudoku::core::SystemTimeProvider>(); });

        // Register real implementations
        container.registerSingleton<sudoku::core::IGameValidator>(
            []() { return std::make_unique<sudoku::core::GameValidator>(); });

        // Register solver (depends on validator, has self-contained backtracking)
        container.registerSingleton<sudoku::core::ISudokuSolver>([&container]() {
            auto validator = container.resolve<sudoku::core::IGameValidator>();
            return std::make_unique<sudoku::core::SudokuSolver>(validator);
        });

        // Register puzzle rater (depends on solver)
        container.registerSingleton<sudoku::core::IPuzzleRater>([&container]() {
            auto solver = container.resolve<sudoku::core::ISudokuSolver>();
            return std::make_unique<sudoku::core::PuzzleRater>(solver);
        });

        // Register puzzle generator (depends on rater)
        container.registerSingleton<sudoku::core::IPuzzleGenerator>([&container]() {
            // Rating re-enabled after fixing infinite loop bugs (commits 45971d2 + 16fa56d)
            // Fixes: applyStep() return value check + MAX_ITERATIONS limit
            auto rater = container.resolve<sudoku::core::IPuzzleRater>();
            return std::make_unique<sudoku::core::PuzzleGenerator>(rater);
        });

        container.registerSingleton<sudoku::core::IStatisticsManager>(
            []() { return std::make_unique<sudoku::core::StatisticsManager>(); });

        container.registerSingleton<sudoku::core::ISaveManager>(
            []() { return std::make_unique<sudoku::core::SaveManager>(); });

        // Register localization manager
        container.registerSingleton<sudoku::core::ILocalizationManager>([]() {
            auto exe_dir = std::filesystem::canonical("/proc/self/exe").parent_path();
            auto locales_dir = exe_dir / "locales";
            auto manager = std::make_unique<sudoku::core::LocalizationManager>(locales_dir);
            auto result = manager->setLocale("en");
            if (!result) {
                spdlog::warn("Failed to load English locale: {}", result.error());
            }
            return manager;
        });

        // Register training exercise generator (depends on puzzle generator + solver)
        container.registerSingleton<sudoku::core::ITrainingExerciseGenerator>([&container]() {
            auto generator = container.resolve<sudoku::core::IPuzzleGenerator>();
            auto solver = container.resolve<sudoku::core::ISudokuSolver>();
            return std::make_unique<sudoku::core::TrainingExerciseGenerator>(generator, solver);
        });

        spdlog::info("Dependency injection container configured with {} registrations",
                     container.getRegistrationCount());
    }

    static std::shared_ptr<sudoku::viewmodel::GameViewModel> createViewModel() {
        auto& container = sudoku::core::getContainer();

        // Resolve dependencies
        auto validator = container.resolve<sudoku::core::IGameValidator>();
        auto generator = container.resolve<sudoku::core::IPuzzleGenerator>();
        auto solver = container.resolve<sudoku::core::ISudokuSolver>();
        auto stats_manager = container.resolve<sudoku::core::IStatisticsManager>();
        auto save_manager = container.resolve<sudoku::core::ISaveManager>();
        auto loc_manager = container.resolve<sudoku::core::ILocalizationManager>();

        // Create view model with injected dependencies
        return std::make_shared<sudoku::viewmodel::GameViewModel>(validator, generator, solver, stats_manager,
                                                                  save_manager, loc_manager);
    }
};

int main(int argc, char* argv[]) {
    // Suppress unused parameter warnings
    (void)argc;
    (void)argv;

    spdlog::info("Starting Sudoku application with new MVVM architecture...");

    auto app = std::make_unique<Application>();

    if (!app->Initialize()) {
        spdlog::error("Failed to initialize application");
        return -1;
    }

    app->Run();
    app->Shutdown();

    spdlog::info("Sudoku application terminated successfully");
    return 0;
}