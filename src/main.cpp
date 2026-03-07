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

#include "core/di_container.h"
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
#include "view/main_window.h"
#include "view_model/game_view_model.h"
#include "view_model/training_view_model.h"

#include <filesystem>
#include <memory>

#include <QApplication>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace {

void setupDependencies() {
    auto& container = sudoku::core::getContainer();

    container.registerSingleton<sudoku::core::ITimeProvider>(
        []() { return std::make_unique<sudoku::core::SystemTimeProvider>(); });

    container.registerSingleton<sudoku::core::IGameValidator>(
        []() { return std::make_unique<sudoku::core::GameValidator>(); });

    container.registerSingleton<sudoku::core::ISudokuSolver>([&container]() {
        auto validator = container.resolve<sudoku::core::IGameValidator>();
        return std::make_unique<sudoku::core::SudokuSolver>(validator);
    });

    container.registerSingleton<sudoku::core::IPuzzleRater>([&container]() {
        auto solver = container.resolve<sudoku::core::ISudokuSolver>();
        return std::make_unique<sudoku::core::PuzzleRater>(solver);
    });

    container.registerSingleton<sudoku::core::IPuzzleGenerator>([&container]() {
        auto rater = container.resolve<sudoku::core::IPuzzleRater>();
        return std::make_unique<sudoku::core::PuzzleGenerator>(rater);
    });

    container.registerSingleton<sudoku::core::IStatisticsManager>(
        []() { return std::make_unique<sudoku::core::StatisticsManager>(); });

    container.registerSingleton<sudoku::core::ISaveManager>(
        []() { return std::make_unique<sudoku::core::SaveManager>(); });

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

    container.registerSingleton<sudoku::core::ITrainingExerciseGenerator>([&container]() {
        auto generator = container.resolve<sudoku::core::IPuzzleGenerator>();
        auto solver = container.resolve<sudoku::core::ISudokuSolver>();
        return std::make_unique<sudoku::core::TrainingExerciseGenerator>(generator, solver);
    });

    spdlog::info("Dependency injection container configured with {} registrations", container.getRegistrationCount());
}

std::shared_ptr<sudoku::viewmodel::GameViewModel> createViewModel() {
    auto& container = sudoku::core::getContainer();

    auto validator = container.resolve<sudoku::core::IGameValidator>();
    auto generator = container.resolve<sudoku::core::IPuzzleGenerator>();
    auto solver = container.resolve<sudoku::core::ISudokuSolver>();
    auto stats_manager = container.resolve<sudoku::core::IStatisticsManager>();
    auto save_manager = container.resolve<sudoku::core::ISaveManager>();
    auto loc_manager = container.resolve<sudoku::core::ILocalizationManager>();

    return std::make_shared<sudoku::viewmodel::GameViewModel>(validator, generator, solver, stats_manager, save_manager,
                                                              loc_manager);
}

}  // namespace

int main(int argc, char* argv[]) {
    // Setup multi-sink logger: console + debug log file (truncated each launch)
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto exe_dir = std::filesystem::canonical("/proc/self/exe").parent_path();
    auto log_path = exe_dir / "sudoku_debug.log";
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.string(), true);  // truncate=true
    auto logger = std::make_shared<spdlog::logger>("sudoku", spdlog::sinks_init_list{console_sink, file_sink});
    logger->set_level(spdlog::level::debug);
    logger->flush_on(spdlog::level::debug);
    spdlog::set_default_logger(logger);

    spdlog::info("Starting Sudoku application with Qt6 + MVVM architecture...");
    spdlog::info("Debug log: {}", log_path.string());

    QApplication qt_app(argc, argv);
    QApplication::setApplicationName("Sudoku");
    QApplication::setApplicationVersion("1.0.0");

    setupDependencies();

    auto view_model = createViewModel();
    auto& container = sudoku::core::getContainer();
    auto loc_manager = container.resolve<sudoku::core::ILocalizationManager>();
    auto exercise_gen = container.resolve<sudoku::core::ITrainingExerciseGenerator>();
    auto training_vm = std::make_shared<sudoku::viewmodel::TrainingViewModel>(exercise_gen, loc_manager);

    sudoku::view::MainWindow main_window;
    main_window.setViewModel(view_model);
    main_window.setLocalizationManager(loc_manager);
    main_window.setTrainingViewModel(training_vm);

    // Try to load auto-save, otherwise start new game
    auto save_manager = container.resolve<sudoku::core::ISaveManager>();
    if (save_manager->hasAutoSave()) {
        spdlog::info("Auto-save detected, attempting to load...");
        auto result = save_manager->loadAutoSave();
        if (result.has_value()) {
            if (result->is_complete) {
                spdlog::info("Auto-save contains completed game, starting fresh");
                view_model->startNewGame(sudoku::core::Difficulty::Medium);
            } else {
                view_model->restoreGameState(result.value());
            }
        } else {
            spdlog::warn("Auto-save load failed: {}, starting new game", static_cast<int>(result.error()));
            view_model->startNewGame(sudoku::core::Difficulty::Medium);
        }
    } else {
        spdlog::info("No auto-save found, starting new Medium game");
        view_model->startNewGame(sudoku::core::Difficulty::Medium);
    }

    main_window.show();

    spdlog::info("Application initialized successfully, entering Qt event loop");
    int result = QApplication::exec();

    // Auto-save on exit
    if (view_model->isGameStateDirty()) {
        view_model->autoSave();
        spdlog::info("Game state saved on exit");
    }

    sudoku::core::getContainer().clear();
    spdlog::info("Sudoku application terminated successfully");
    return result;
}
