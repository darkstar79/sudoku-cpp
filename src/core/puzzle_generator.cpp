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

#include "puzzle_generator.h"

#include "backtracking_solver.h"
#include "constraint_state.h"
#include "core/board.h"
#include "core/board_utils.h"
#include "cpu_features.h"
#include "puzzle_rating.h"
#include "simd_constraint_state.h"

#include <algorithm>
#include <chrono>
#include <random>
#include <utility>

#include <bit>
#include <spdlog/spdlog.h>

namespace sudoku::core {

// Clue count ranges for each difficulty level
namespace {
constexpr int EASY_MIN_CLUES = 36;
constexpr int EASY_MAX_CLUES = 40;
constexpr int MEDIUM_MIN_CLUES = 28;
constexpr int MEDIUM_MAX_CLUES = 35;
constexpr int HARD_MIN_CLUES = 22;
constexpr int HARD_MAX_CLUES = 27;
constexpr int EXPERT_MIN_CLUES = 17;
constexpr int EXPERT_MAX_CLUES = 27;  // Clue count doesn't determine difficulty — rating does
constexpr int MASTER_MIN_CLUES = 17;
constexpr int MASTER_MAX_CLUES = 27;  // Same as Expert — rating determines difficulty

// Default timeout for solution counting (30 seconds)
constexpr int DEFAULT_SOLUTION_TIMEOUT_MS = 30000;
}  // namespace

PuzzleGenerator::PuzzleGenerator() {
    initializeZobristTable();
}

PuzzleGenerator::PuzzleGenerator(std::shared_ptr<IPuzzleRater> rater) : rater_(std::move(rater)) {
    initializeZobristTable();
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — top-level orchestration over difficulty-specific strategies; branching is inherent to the generation flow
std::expected<Puzzle, GenerationError> PuzzleGenerator::generatePuzzle(const GenerationSettings& settings) const {
    // Validate input
    if (settings.difficulty < Difficulty::Easy || settings.difficulty > Difficulty::Master) {
        spdlog::error("Invalid difficulty: {}", static_cast<int>(settings.difficulty));
        return std::unexpected(GenerationError::InvalidDifficulty);
    }

    // Apply solver path from settings (if specified)
    active_solver_path_ = settings.solver_path.value_or(SolverPath::Auto);

    spdlog::debug("Generating puzzle with difficulty: {}, ensure_unique: {}, max_attempts: {}, solver_path: {}",
                  static_cast<int>(settings.difficulty), settings.ensure_unique, settings.max_attempts,
                  solverPathName(active_solver_path_));

    // Set up random number generator
    std::mt19937 rng(settings.seed != 0 ? settings.seed : std::random_device{}());

    // Retry loop for puzzle generation
    for (int attempt = 0; attempt < settings.max_attempts; ++attempt) {
        // Clear memoization cache once per attempt (not per countSolutions call)
        // This allows cache to accumulate during clue removal within a single attempt
        clearCache();

        // Generate complete solution
        spdlog::debug("Attempt {}/{}: Generating complete solution...", attempt + 1, settings.max_attempts);
        auto solution = generateCompleteSolution(rng);
        if (solution.empty()) {
            spdlog::error("Failed to generate complete solution on attempt {}", attempt + 1);
            continue;  // Try again
        }
        spdlog::debug("Complete solution generated successfully");

        // Remove clues to create puzzle
        // Use iterative deepening for Expert, standard greedy for others
        spdlog::debug("Removing clues to create puzzle...");
        auto puzzle_board = removeCluesToCreatePuzzleIterative(solution, settings, rng);
        if (puzzle_board.empty()) {
            spdlog::error("Failed to remove clues to create puzzle on attempt {}", attempt + 1);
            continue;  // Try again
        }
        int final_clue_count = countClues(puzzle_board);
        spdlog::debug("Clues removed successfully, final clue count: {}, checking unique solution...",
                      final_clue_count);

        // Verify unique solution if required
        if (settings.ensure_unique && !hasUniqueSolution(puzzle_board)) {
            spdlog::debug("Generated puzzle does not have unique solution (final clue count: {}), retrying...",
                          final_clue_count);
            continue;  // Try again
        }

        // Success! Create puzzle result
        spdlog::info("Successfully generated puzzle on attempt {}/{}", attempt + 1, settings.max_attempts);
        Puzzle result;
        result.board = std::move(puzzle_board);
        result.solution = std::move(solution);
        result.difficulty = settings.difficulty;
        result.seed = settings.seed;
        result.clue_count = countClues(result.board);

        // Rate puzzle if rater is available
        if (rater_) {
            auto rating_result = rater_->ratePuzzle(result.board);
            if (rating_result.has_value()) {
                result.rating = rating_result->total_score;
                result.requires_backtracking = rating_result->requires_backtracking;
                for (const auto& step : rating_result->solve_path) {
                    if (step.technique != SolvingTechnique::Backtracking) {
                        result.required_techniques.insert(step.technique);
                    }
                }
                spdlog::info("Puzzle rated: {} points (Sudoku Explainer scale)", result.rating);

                // Validate rating: reject puzzles outside the expected range for their difficulty
                {
                    auto [min_rating, max_rating] = getDifficultyRatingRange(settings.difficulty);
                    if (result.rating < min_rating || result.rating >= max_rating) {
                        spdlog::debug("Puzzle rating {} outside range [{}, {}), rejecting...", result.rating,
                                      min_rating, max_rating);
                        continue;  // Try again with different puzzle
                    }
                    spdlog::debug("Puzzle rating {} within range [{}, {})", result.rating, min_rating, max_rating);
                }
            } else {
                spdlog::warn("Failed to rate puzzle: {}", static_cast<int>(rating_result.error()));
                // Continue without rating if rating fails (don't reject puzzle)
            }
        }

        return result;
    }

    // All attempts failed
    spdlog::error("Failed to generate puzzle after {} attempts", settings.max_attempts);
    return std::unexpected(GenerationError::NoUniqueSolution);
}

std::expected<Puzzle, GenerationError> PuzzleGenerator::generatePuzzle(Difficulty difficulty) const {
    GenerationSettings settings;
    settings.difficulty = difficulty;
    settings.seed = 0;  // Random seed
    settings.ensure_unique = true;
    settings.max_attempts = 1000;

    return generatePuzzle(settings);
}

std::expected<std::vector<std::vector<int>>, GenerationError>
PuzzleGenerator::solvePuzzle(const std::vector<std::vector<int>>& board) const {
    if (!validatePuzzle(board)) {
        return std::unexpected(GenerationError::GenerationFailed);
    }

    auto solution = board;  // Copy the input board
    if (solvePuzzleBacktrack(solution)) {
        return solution;
    }

    return std::unexpected(GenerationError::GenerationFailed);
}

bool PuzzleGenerator::hasUniqueSolution(const std::vector<std::vector<int>>& board) const {
    // Phase 3: Propagation integration removed - creates too much overhead
    // The existing solver already applies hidden singles and naked singles efficiently
    return countSolutions(board, 2) == 1;
}

int PuzzleGenerator::countClues(const std::vector<std::vector<int>>& board) const {
    int count = 0;
    for (const auto& row : board) {
        for (int cell : row) {
            if (cell != 0) {
                ++count;
            }
        }
    }
    return count;
}

bool PuzzleGenerator::validatePuzzle(const std::vector<std::vector<int>>& board) const {
    // Check board dimensions
    if (board.size() != 9) {
        return false;
    }
    for (const auto& row : board) {
        if (row.size() != 9) {
            return false;
        }
    }

    // Check that all values are 0-9
    for (const auto& row : board) {
        for (int cell : row) {
            if (cell < 0 || cell > 9) {
                return false;
            }
        }
    }

    // Check that current state has no conflicts
    return validator_.validateBoard(board);
}

std::vector<std::vector<int>> PuzzleGenerator::generateCompleteSolution(std::mt19937& rng) const {
    auto board = createEmptyBoard();

    if (fillBoardRecursively(board, rng)) {
        return board;
    }

    return {};  // Return empty board if generation failed
}

int PuzzleGenerator::runRemovalOrdering(std::vector<std::vector<int>>& puzzle,
                                        const std::vector<std::pair<size_t, size_t>>& positions, bool ensure_unique,
                                        int min_clues, int /*max_clues*/) const {
    for (const auto& [row, col] : positions) {
        if (puzzle[row][col] == 0) {
            continue;  // Already empty
        }

        int original_value = puzzle[row][col];
        puzzle[row][col] = 0;  // Remove clue

        // Check uniqueness on every removal — not just near target.
        // Skipping checks above max_clues caused non-unique puzzles to slip through.
        if (ensure_unique && !hasUniqueSolution(puzzle)) {
            puzzle[row][col] = original_value;  // Restore — non-unique
            continue;
        }

        if (countClues(puzzle) <= min_clues) {
            break;  // Reached minimum, stop removing
        }
    }
    return countClues(puzzle);
}

void PuzzleGenerator::runRecompletionPhase(std::vector<std::vector<int>>& best_puzzle, int& best_clue_count,
                                           std::vector<std::pair<size_t, size_t>>& positions, bool ensure_unique,
                                           int min_clues, int max_clues, int num_orderings, std::mt19937& rng) const {
    constexpr int MAX_RECOMPLETIONS = 5;

    auto tryOrdering = [&](const std::vector<std::vector<int>>& start) {
        auto puzzle = start;
        std::shuffle(positions.begin(), positions.end(), rng);
        int final_clues = runRemovalOrdering(puzzle, positions, ensure_unique, min_clues, max_clues);
        spdlog::debug("Re-completion ordering: {} clues remaining (target: {}-{})", final_clues, min_clues, max_clues);
        if (final_clues < best_clue_count) {
            best_clue_count = final_clues;
            best_puzzle = puzzle;
        }
    };

    for (int recomp = 0; recomp < MAX_RECOMPLETIONS; ++recomp) {
        auto recomp_puzzle = best_puzzle;
        int clues_to_drop = 2 + static_cast<int>(rng() % 2);
        for (const auto& pos : selectCluesForDropping(recomp_puzzle, clues_to_drop, rng)) {
            recomp_puzzle[pos.row][pos.col] = 0;
        }
        spdlog::debug("Re-completion {}/{}: Dropped {} clues", recomp + 1, MAX_RECOMPLETIONS, clues_to_drop);

        auto new_solution_result = solvePuzzle(recomp_puzzle);
        if (!new_solution_result.has_value()) {
            spdlog::debug("Re-completion {}/{}: Failed to find new solution", recomp + 1, MAX_RECOMPLETIONS);
            continue;
        }

        const auto& new_solution = new_solution_result.value();
        spdlog::debug("Re-completion {}/{}: Found new solution, retrying removal", recomp + 1, MAX_RECOMPLETIONS);

        for (int ordering = 0; ordering < num_orderings; ++ordering) {
            tryOrdering(new_solution);
            if (best_clue_count >= min_clues && best_clue_count <= max_clues) {
                spdlog::debug("Re-completion: Target range achieved ({} clues)", best_clue_count);
                return;
            }
        }
    }
}

std::vector<std::vector<int>> PuzzleGenerator::removeCluesToCreatePuzzle(const std::vector<std::vector<int>>& solution,
                                                                         const GenerationSettings& settings,
                                                                         std::mt19937& rng) const {
    auto [min_clues, max_clues] = getClueRange(settings.difficulty);

    // Determine number of removal orderings based on difficulty
    // Hard/Expert: try multiple orderings to escape local maxima in greedy clue removal
    // Easy/Medium: single ordering (success rate already near 100%)
    int num_orderings = 1;
    if (settings.difficulty == Difficulty::Expert || settings.difficulty == Difficulty::Master) {
        num_orderings = 10;
    } else if (settings.difficulty == Difficulty::Hard) {
        num_orderings = 3;
    }

    std::vector<std::vector<int>> best_puzzle = solution;
    int best_clue_count = BOARD_SIZE * BOARD_SIZE;  // Start at 81

    // Build position list once; re-shuffle each ordering iteration
    std::vector<std::pair<size_t, size_t>> positions;
    positions.reserve(BOARD_SIZE * BOARD_SIZE);
    forEachCell([&](size_t row, size_t col) { positions.emplace_back(row, col); });

    // Runs one ordering from a given solution; updates best_puzzle/best_clue_count
    auto tryOrdering = [&](const std::vector<std::vector<int>>& start) {
        auto puzzle = start;
        std::shuffle(positions.begin(), positions.end(), rng);
        int final_clues = runRemovalOrdering(puzzle, positions, settings.ensure_unique, min_clues, max_clues);
        spdlog::debug("Clue removal ordering: {} clues remaining (target: {}-{})", final_clues, min_clues, max_clues);
        if (final_clues < best_clue_count) {
            best_clue_count = final_clues;
            best_puzzle = puzzle;
        }
    };

    // Try multiple removal orderings
    for (int ordering = 0; ordering < num_orderings; ++ordering) {
        tryOrdering(solution);
        if (best_clue_count >= min_clues && best_clue_count <= max_clues) {
            spdlog::debug("Target clue range achieved ({} clues), stopping after {} orderings", best_clue_count,
                          ordering + 1);
            break;
        }
    }

    // Re-completion strategy: If stuck above max_clues, try finding new solution paths
    // Only enabled for Expert difficulty to avoid unnecessary work for easier puzzles
    if ((settings.difficulty == Difficulty::Expert || settings.difficulty == Difficulty::Master) &&
        best_clue_count > max_clues) {
        spdlog::debug("Re-completion: Best puzzle ({} clues) above target max ({}), attempting re-completion strategy",
                      best_clue_count, max_clues);
        runRecompletionPhase(best_puzzle, best_clue_count, positions, settings.ensure_unique, min_clues, max_clues,
                             num_orderings, rng);
        spdlog::debug("Re-completion complete: Best puzzle has {} clues (target: {}-{})", best_clue_count, min_clues,
                      max_clues);
    }

    spdlog::debug("Final best puzzle: {} clues (target: {}-{})", best_clue_count, min_clues, max_clues);
    return best_puzzle;
}

// Phase 1: Iterative Deepening Implementation

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — iterative clue removal with uniqueness checking; branching is inherent to puzzle generation
std::vector<std::vector<int>> PuzzleGenerator::removeCluesToTarget(const std::vector<std::vector<int>>& solution,
                                                                   int target_clues, int max_attempts,
                                                                   std::mt19937& rng) const {
    // Early exit: if target equals current clues, return solution
    int current_clues = countClues(solution);
    if (current_clues == target_clues) {
        return solution;
    }

    // Early exit: if max_attempts is zero, return empty board
    if (max_attempts == 0) {
        return createEmptyBoard();
    }

    spdlog::debug("removeCluesToTarget: Attempting to reach {} clues (from {}) in {} attempts", target_clues,
                  current_clues, max_attempts);

    std::vector<std::vector<int>> best_puzzle = solution;
    int best_clue_count = current_clues;

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        auto puzzle = solution;

        // Create shuffled list of all positions
        std::vector<std::pair<size_t, size_t>> all_positions;
        all_positions.reserve(BOARD_SIZE * BOARD_SIZE);

        forEachCell([&](size_t row, size_t col) { all_positions.emplace_back(row, col); });

        std::shuffle(all_positions.begin(), all_positions.end(), rng);

        // Greedy removal with uniqueness checking
        int clues_remaining = countClues(puzzle);
        int restorations = 0;

        for (const auto& [row, col] : all_positions) {
            if (clues_remaining <= target_clues) {
                break;  // Reached target
            }

            if (puzzle[row][col] == 0) {
                continue;  // Already empty
            }

            int original_value = puzzle[row][col];
            puzzle[row][col] = 0;
            clues_remaining--;  // Optimistically decrement

            // Only check uniqueness when within buffer zone of target
            // This allows aggressive removal early on while maintaining uniqueness near target
            constexpr int UNIQUENESS_CHECK_BUFFER = 15;
            bool near_target = (clues_remaining - target_clues) <= UNIQUENESS_CHECK_BUFFER;

            if (near_target && !hasUniqueSolution(puzzle)) {
                // Restore clue - this removal breaks uniqueness
                puzzle[row][col] = original_value;
                clues_remaining++;  // Undo the optimistic decrement
                restorations++;
            } else {
                // Success: reached exact target
                if (clues_remaining == target_clues) {
                    // Final uniqueness verification
                    if (hasUniqueSolution(puzzle)) {
                        spdlog::debug("Attempt {}/{}: SUCCESS - Reached target {} clues (restorations: {})",
                                      attempt + 1, max_attempts, target_clues, restorations);
                        return puzzle;
                    }
                    // Target reached but not unique, restore and continue
                    puzzle[row][col] = original_value;
                    clues_remaining++;
                    restorations++;
                }
            }
        }

        // Check final result
        int final_clues = countClues(puzzle);
        if (final_clues == target_clues && hasUniqueSolution(puzzle)) {
            spdlog::debug("Attempt {}/{}: SUCCESS - Reached target {} clues", attempt + 1, max_attempts, target_clues);
            return puzzle;
        }

        spdlog::trace("Attempt {}/{}: Final {} clues (target: {}, restorations: {})", attempt + 1, max_attempts,
                      final_clues, target_clues, restorations);
    }

    // Failed to reach exact target
    spdlog::debug("removeCluesToTarget: Failed to reach {} clues after {} attempts (best: {})", target_clues,
                  max_attempts, best_clue_count);
    return createEmptyBoard();  // Return empty board to indicate failure
}

std::vector<std::vector<int>>
PuzzleGenerator::removeCluesToCreatePuzzleIterative(const std::vector<std::vector<int>>& solution,
                                                    const GenerationSettings& settings, std::mt19937& rng) const {
    // All difficulties use standard greedy algorithm with multiple orderings + re-completion.
    // Iterative deepening was removed: it optimized for minimum clue count, which does NOT
    // correlate with difficulty. A 17-clue puzzle solvable by naked/hidden singles is Easy,
    // not Expert. Actual difficulty validation (required solving techniques) is handled by
    // the rating check in generatePuzzle().
    return removeCluesToCreatePuzzle(solution, settings, rng);
}

// ============================================================================
// Phase 2: Intelligent Clue Dropping (for improved re-completion strategy)
// ============================================================================

std::vector<ClueAnalysis> PuzzleGenerator::analyzeClueConstraints(const std::vector<std::vector<int>>& board) {
    std::vector<ClueAnalysis> analysis;
    analysis.reserve(BOARD_SIZE * BOARD_SIZE);  // Reserve max possible

    // Iterate over all cells
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            int value = board[row][col];

            // Skip empty cells (only analyze clues)
            if (value == 0) {
                continue;
            }

            // Count how many alternative values could be placed here
            int alternative_count = 0;

            // Try all values 1-9
            for (int test_value = MIN_VALUE; test_value <= MAX_VALUE; ++test_value) {
                // Skip the current value (we're counting alternatives)
                if (test_value == value) {
                    continue;
                }

                // Check if this value would be valid at this position
                Position pos{.row = row, .col = col};
                if (!GameValidator::hasConflict(board, pos, test_value)) {
                    alternative_count++;
                }
            }

            // Compute constraint score: higher score = more constrained (fewer alternatives)
            int constraint_score = MAX_VALUE - alternative_count;

            // Add to analysis
            analysis.push_back(ClueAnalysis{.pos = Position{.row = row, .col = col},
                                            .value = value,
                                            .alternative_count = alternative_count,
                                            .constraint_score = constraint_score});
        }
    }

    // Sort by constraint_score descending (most constrained first)
    std::ranges::sort(analysis, [](const ClueAnalysis& lhs, const ClueAnalysis& rhs) {
        return lhs.constraint_score > rhs.constraint_score;
    });

    spdlog::debug("analyzeClueConstraints: Analyzed {} clues", analysis.size());

    return analysis;
}

std::vector<Position> PuzzleGenerator::selectCluesForDropping(const std::vector<std::vector<int>>& board, int num_clues,
                                                              std::mt19937& rng) {
    // Handle edge cases
    if (num_clues <= 0) {
        return {};
    }

    // Analyze all clues to get constraint scores
    auto analysis = analyzeClueConstraints(board);

    // Cap at total number of clues available
    int clues_to_select = std::min(num_clues, static_cast<int>(analysis.size()));

    std::vector<Position> selected;
    selected.reserve(clues_to_select);

    // Strategy: Prioritize highly constrained clues (higher score) for dropping
    // This creates more diverse re-completion paths (better exploration)
    //
    // Use weighted random selection from top candidates to introduce randomness
    // while still preferring highly constrained clues

    // Take top 2*clues_to_select candidates (or all if fewer available)
    int candidate_pool_size = std::min(clues_to_select * 2, static_cast<int>(analysis.size()));

    // Create a pool of candidates from the most constrained clues
    std::vector<ClueAnalysis> candidate_pool;
    candidate_pool.reserve(candidate_pool_size);

    for (int i = 0; i < candidate_pool_size; ++i) {
        candidate_pool.push_back(analysis[i]);
    }

    // Shuffle candidates to introduce randomness
    std::shuffle(candidate_pool.begin(), candidate_pool.end(), rng);

    // Select the requested number
    for (int i = 0; std::cmp_less(i, clues_to_select) && std::cmp_less(i, candidate_pool.size()); ++i) {
        selected.push_back(candidate_pool[i].pos);
    }

    spdlog::debug("selectCluesForDropping: Selected {} clues from {} candidates (requested: {})", selected.size(),
                  candidate_pool_size, num_clues);

    return selected;
}

bool PuzzleGenerator::solvePuzzleBacktrack(std::vector<std::vector<int>>& board) const {
    // Use unified BacktrackingSolver with sequential strategy (deterministic)
    auto validator_ptr = std::make_shared<GameValidator>(validator_);
    BacktrackingSolver solver(validator_ptr);
    return solver.solve(board, ValueSelectionStrategy::Sequential);
}

SolverPath PuzzleGenerator::resolveEffectivePath() const {
    SolverPath path = active_solver_path_;
    if (path == SolverPath::Auto) {
        if (hasAvx512()) {
            path = SolverPath::AVX512;
        } else if (hasAvx2()) {
            path = SolverPath::AVX2;
        } else {
            path = SolverPath::Scalar;
        }
    }
    // Fall back if requested path is unavailable
    if (!isSolverPathAvailable(path)) {
        if (hasAvx2()) {
            path = SolverPath::AVX2;
        } else {
            path = SolverPath::Scalar;
        }
    }
    return path;
}

int PuzzleGenerator::countSolutions(const std::vector<std::vector<int>>& board, int max_solutions) const {
    // Note: Cache is now cleared per-attempt in generatePuzzle(), not per-call here
    // This allows cache accumulation across multiple countSolutions() calls within
    // a single generation attempt, improving performance when checking uniqueness
    // after each clue removal.

    // Convert to Board for hot path: 96-byte stack copy instead of 9 heap allocations
    auto board_copy = Board::fromVectors(board);
    int count = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(DEFAULT_SOLUTION_TIMEOUT_MS);
    bool timed_out = false;

    // Compute initial Zobrist hash once; passed incrementally through recursion
    uint64_t hash = computeBoardHash(board_copy);
    uint32_t recursion_count = 0;

    SolverPath effective_path = resolveEffectivePath();

    switch (effective_path) {
        case SolverPath::AVX512:
        case SolverPath::AVX2: {
            SIMDConstraintState simd_state;
            if (effective_path == SolverPath::AVX512) {
                simd_state.setSimdLevel(SolverPath::AVX512);
            }
            simd_state.initFromBoard(board_copy);
            countSolutionsHelperSIMD(board_copy, simd_state, count, max_solutions, start_time, timeout, timed_out, hash,
                                     recursion_count, SIMDConstraintState::ALL_REGIONS_DIRTY);
            break;
        }
        case SolverPath::Scalar:
        case SolverPath::Auto:
        default: {
            ConstraintState state(board_copy);
            countSolutionsHelper(board_copy, state, count, max_solutions, start_time, timeout, timed_out, hash,
                                 recursion_count);
            break;
        }
    }

    // If timed out, assume unique (optimistic approach for normal puzzles)
    // Most timeouts happen on Easy/Medium puzzles that actually ARE unique
    if (timed_out) {
        return 1;  // Return 1 to assume unique on timeout
    }
    return count;
}

int PuzzleGenerator::countSolutionsWithTimeout(const std::vector<std::vector<int>>& board, int max_solutions,
                                               std::chrono::milliseconds timeout) const {
    // Note: Cache is now cleared per-attempt in generatePuzzle(), not per-call here
    // This allows cache accumulation across multiple countSolutions() calls within
    // a single generation attempt, improving performance when checking uniqueness
    // after each clue removal.

    // Convert to Board for hot path: 96-byte stack copy instead of 9 heap allocations
    auto board_copy = Board::fromVectors(board);
    int count = 0;
    auto start_time = std::chrono::steady_clock::now();
    bool timed_out = false;

    // Compute initial Zobrist hash once; passed incrementally through recursion
    uint64_t hash = computeBoardHash(board_copy);
    uint32_t recursion_count = 0;

    SolverPath effective_path = resolveEffectivePath();

    switch (effective_path) {
        case SolverPath::AVX512:
        case SolverPath::AVX2: {
            SIMDConstraintState simd_state;
            if (effective_path == SolverPath::AVX512) {
                simd_state.setSimdLevel(SolverPath::AVX512);
            }
            simd_state.initFromBoard(board_copy);
            countSolutionsHelperSIMD(board_copy, simd_state, count, max_solutions, start_time, timeout, timed_out, hash,
                                     recursion_count, SIMDConstraintState::ALL_REGIONS_DIRTY);
            break;
        }
        case SolverPath::Scalar:
        case SolverPath::Auto:
        default: {
            ConstraintState state(board_copy);
            countSolutionsHelper(board_copy, state, count, max_solutions, start_time, timeout, timed_out, hash,
                                 recursion_count);
            break;
        }
    }

    // If timed out, assume non-unique (conservative approach)
    if (timed_out) {
        return max_solutions;  // Return max to indicate non-unique
    }
    return count;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — performance-critical recursive backtracking hot path; monolithic for inlining; refactoring adds call overhead on inner loop
void PuzzleGenerator::countSolutionsHelper(Board& board, ConstraintState& state, int& count, int max_solutions,
                                           const std::chrono::steady_clock::time_point& start_time,
                                           const std::chrono::milliseconds& timeout, bool& timed_out, uint64_t hash,
                                           uint32_t& recursion_count) const {
    // Sample timeout check every 1024 iterations to avoid vDSO overhead (was 9% of profile)
    if ((++recursion_count & 0x3FF) == 0) {
        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
        if (elapsed > timeout) {
            timed_out = true;
            return;
        }
    }

    if (count >= max_solutions) {
        return;  // Early termination - found enough solutions
    }

    // Memoization cache lookup (O(1) per recursion via incremental Zobrist hash)
    const int* cached = solution_count_cache_.find(hash);
    if (cached != nullptr) {
        count += *cached;
        return;
    }

    // Hidden singles detection: find value that can only go in one position within a region
    auto hidden_single = state.findHiddenSingle(board);
    if (hidden_single.has_value()) {
        const auto& [pos, value] = hidden_single.value();

        board[pos.row][pos.col] = static_cast<int8_t>(value);
        state.placeValue(pos.row, pos.col, value);

        int count_before = count;
        uint64_t hs_hash = hash ^ zobrist_table_[pos.row][pos.col][0] ^ zobrist_table_[pos.row][pos.col][value];
        countSolutionsHelper(board, state, count, max_solutions, start_time, timeout, timed_out, hs_hash,
                             recursion_count);

        if (timed_out) {
            board[pos.row][pos.col] = 0;
            state.removeValue(pos.row, pos.col, value);
            return;
        }

        board[pos.row][pos.col] = 0;
        state.removeValue(pos.row, pos.col, value);

        // Only cache if we fully explored the subtree (not truncated by max_solutions)
        if (count < max_solutions) {
            solution_count_cache_.insert(hash, count - count_before);
        }
        return;
    }

    // MCV heuristic: select cell with fewest legal values for optimal pruning
    // Uses ConstraintState::countPossibleValues() — O(1) via POPCNT, zero heap allocations
    size_t best_row = 0;
    size_t best_col = 0;
    int min_domain = MAX_VALUE + 1;
    bool found_empty = false;

    for (size_t i = 0; i < TOTAL_CELLS; ++i) {
        if (board.cells[i] == 0) {
            size_t row = i / BOARD_SIZE;
            size_t col = i % BOARD_SIZE;
            int domain = state.countPossibleValues(row, col);

            if (domain <= 1) {
                // domain==1: naked single (forced move), domain==0: dead-end
                // Either way, this is the optimal pick — stop searching
                best_row = row;
                best_col = col;
                min_domain = domain;
                found_empty = true;
                break;
            }

            if (domain < min_domain) {
                min_domain = domain;
                best_row = row;
                best_col = col;
                found_empty = true;
            }
        }
    }

    if (!found_empty) {
        ++count;
        solution_count_cache_.insert(hash, 1);
        return;
    }

    Position pos{.row = best_row, .col = best_col};

    // Naked singles: cell with exactly 1 candidate is a forced move
    if (min_domain == 1) {
        uint16_t mask = state.getPossibleValuesMask(pos.row, pos.col);
        int forced_value = std::countr_zero(static_cast<unsigned>(mask));  // Bits 1-9 represent digits 1-9

        board[pos.row][pos.col] = static_cast<int8_t>(forced_value);
        state.placeValue(pos.row, pos.col, forced_value);
        int count_before = count;

        uint64_t ns_hash = hash ^ zobrist_table_[pos.row][pos.col][0] ^ zobrist_table_[pos.row][pos.col][forced_value];
        countSolutionsHelper(board, state, count, max_solutions, start_time, timeout, timed_out, ns_hash,
                             recursion_count);

        if (timed_out) {
            board[pos.row][pos.col] = 0;
            state.removeValue(pos.row, pos.col, forced_value);
            return;
        }

        board[pos.row][pos.col] = 0;
        state.removeValue(pos.row, pos.col, forced_value);

        // Only cache if we fully explored the subtree (not truncated by max_solutions)
        if (count < max_solutions) {
            solution_count_cache_.insert(hash, count - count_before);
        }
        return;
    }

    // Try all valid values using O(1) ConstraintState validation
    int solutions_from_this_state = 0;

    for (int num = MIN_VALUE; num <= MAX_VALUE; ++num) {
        if (state.isAllowed(pos.row, pos.col, num)) {
            board[pos.row][pos.col] = static_cast<int8_t>(num);
            state.placeValue(pos.row, pos.col, num);
            int count_before = count;

            uint64_t new_hash = hash ^ zobrist_table_[pos.row][pos.col][0] ^ zobrist_table_[pos.row][pos.col][num];
            countSolutionsHelper(board, state, count, max_solutions, start_time, timeout, timed_out, new_hash,
                                 recursion_count);

            if (timed_out) {
                board[pos.row][pos.col] = 0;
                state.removeValue(pos.row, pos.col, num);
                return;
            }

            solutions_from_this_state += (count - count_before);
            board[pos.row][pos.col] = 0;
            state.removeValue(pos.row, pos.col, num);

            if (count >= max_solutions) {
                break;
            }
        }
    }

    // Only cache if we fully explored all branches (not truncated by max_solutions)
    if (count < max_solutions) {
        solution_count_cache_.insert(hash, solutions_from_this_state);
    }
}

bool PuzzleGenerator::fillBoardRecursively(std::vector<std::vector<int>>& board, std::mt19937& rng) const {
    // Use unified BacktrackingSolver with randomized strategy (for puzzle generation)
    auto validator_ptr = std::make_shared<GameValidator>(validator_);
    BacktrackingSolver solver(validator_ptr);
    return solver.solve(board, ValueSelectionStrategy::Randomized, &rng);
}

std::pair<int, int> PuzzleGenerator::getClueRange(Difficulty difficulty) {
    switch (difficulty) {
        case Difficulty::Easy:
            return {EASY_MIN_CLUES, EASY_MAX_CLUES};
        case Difficulty::Medium:
            return {MEDIUM_MIN_CLUES, MEDIUM_MAX_CLUES};
        case Difficulty::Hard:
            return {HARD_MIN_CLUES, HARD_MAX_CLUES};
        case Difficulty::Expert:
            return {EXPERT_MIN_CLUES, EXPERT_MAX_CLUES};
        case Difficulty::Master:
            return {MASTER_MIN_CLUES, MASTER_MAX_CLUES};
        default:
            return {MEDIUM_MIN_CLUES, MEDIUM_MAX_CLUES};  // Default to medium
    }
}

std::optional<Position> PuzzleGenerator::findEmptyPosition(const std::vector<std::vector<int>>& board,
                                                           bool use_mcv_heuristic) const {
    if (!use_mcv_heuristic) {
        // Sequential scan: Returns first empty cell (backward compatible)
        std::optional<Position> result = std::nullopt;
        anyCell([&](size_t row, size_t col) {
            if (board[row][col] == 0) {
                result = Position{.row = row, .col = col};
                return true;  // Found empty cell, stop searching
            }
            return false;  // Continue searching
        });
        return result;
    }

    // MCV (Most Constrained Variable) heuristic:
    // Select the empty cell with the fewest legal values
    // This reduces branching factor and causes earlier constraint violations
    std::optional<Position> best_position = std::nullopt;
    size_t min_domain_size = MAX_VALUE + 1;  // 10 (impossible size)

    bool early_return = false;
    forEachCell([&](size_t row, size_t col) {
        if (early_return) {
            return;  // Skip remaining cells after early termination
        }
        if (board[row][col] == 0) {
            Position pos{.row = row, .col = col};
            auto possible = validator_.getPossibleValues(board, pos);
            size_t domain_size = possible.size();

            // Early termination: domain size = 1 is optimal (forced move)
            if (domain_size == 1) {
                best_position = pos;
                early_return = true;
                return;
            }

            // Dead-end: domain size = 0 means cell is unsolvable
            // Return immediately to trigger early backtracking
            if (domain_size == 0) {
                best_position = pos;
                early_return = true;
                return;
            }

            if (domain_size < min_domain_size) {
                min_domain_size = domain_size;
                best_position = pos;
            }
        }
    });

    return best_position;
}

std::optional<Position> PuzzleGenerator::findEmptyPosition(const Board& board, bool use_mcv_heuristic) const {
    if (!use_mcv_heuristic) {
        // Sequential scan using flat iteration (more efficient than nested loops)
        for (size_t i = 0; i < TOTAL_CELLS; ++i) {
            if (board.cells[i] == 0) {
                return Position{.row = i / BOARD_SIZE, .col = i % BOARD_SIZE};
            }
        }
        return std::nullopt;
    }

    // MCV heuristic requires validator API (vector<vector<int>>), delegate
    auto vec = board.toVectors();
    return findEmptyPosition(vec, true);
}

std::vector<int> PuzzleGenerator::getShuffledNumbers(std::mt19937& rng) {
    std::vector<int> numbers;
    numbers.reserve(MAX_VALUE - MIN_VALUE + 1);
    for (int i = MIN_VALUE; i <= MAX_VALUE; ++i) {
        numbers.push_back(i);
    }
    std::shuffle(numbers.begin(), numbers.end(), rng);
    return numbers;
}

std::vector<std::vector<int>> PuzzleGenerator::createEmptyBoard() {
    return std::vector<std::vector<int>>(BOARD_SIZE, std::vector<int>(BOARD_SIZE, 0));
}

void PuzzleGenerator::initializeZobristTable() {
    // Use fixed seed for Zobrist table (must be consistent across runs)
    // NOLINTNEXTLINE(cert-msc32-c,cert-msc51-cpp) - Deterministic seed required for Zobrist hashing
    std::mt19937_64 rng(0x123456789ABCDEF0ULL);
    std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);

    // Generate random 64-bit values for each (row, col, value) combination
    // Index 0 represents empty cell, indices 1-9 represent values 1-9
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            for (int value = 0; value <= MAX_VALUE; ++value) {
                zobrist_table_[row][col][value] = dist(rng);
            }
        }
    }
}

uint64_t PuzzleGenerator::computeBoardHash(const Board& board) const {
    uint64_t hash = 0;

    // XOR hash values for each filled cell (flat iteration over Board)
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            auto value = static_cast<size_t>(static_cast<unsigned char>(board[row][col]));
            // XOR with the zobrist value for this (row, col, value) combination
            hash ^= zobrist_table_[row][col][value];
        }
    }

    return hash;
}

void PuzzleGenerator::clearCache() const {
    solution_count_cache_.clear();
}

// ============================================================================
// SIMD-OPTIMIZED SOLUTION COUNTING (AVX2)
// ============================================================================
// This implementation uses SIMDConstraintState for faster:
// - MRV heuristic: SIMD popcount finds cell with min candidates in 6 iterations
// - Candidate enumeration: Bitmask iteration with CTZ instead of 1-9 loop
// ============================================================================

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — SIMD hot path; save/restore of 20-peer candidate arrays requires explicit inline code; same structure as scalar version
void PuzzleGenerator::countSolutionsHelperSIMD(Board& board, SIMDConstraintState& state, int& count, int max_solutions,
                                               const std::chrono::steady_clock::time_point& start_time,
                                               const std::chrono::milliseconds& timeout, bool& timed_out, uint64_t hash,
                                               uint32_t& recursion_count, uint32_t dirty_regions) const {
    // Sample timeout check every 1024 iterations to avoid vDSO overhead (was 9% of profile)
    if ((++recursion_count & 0x3FF) == 0) {
        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
        if (elapsed > timeout) {
            timed_out = true;
            return;
        }
    }

    if (count >= max_solutions) {
        return;
    }

    // Memoization cache lookup (O(1) per recursion via incremental Zobrist hash)
    const int* cached = solution_count_cache_.find(hash);
    if (cached != nullptr) {
        count += *cached;
        return;
    }

    // OPTIMIZATION: Hidden singles detection (CRITICAL for performance)
    // Fast path: scan only dirty regions (3 vs 27) for hidden singles created by the last placement.
    // If nothing found in dirty regions, fall back to full scan for pre-existing hidden singles
    // that the parent's early-return scan didn't reach (e.g., in cols/boxes after row hit).
    auto [hs_cell, hs_digit] = state.findHiddenSingle(board, dirty_regions);
    if (hs_cell < 0 && dirty_regions != SIMDConstraintState::ALL_REGIONS_DIRTY) {
        std::tie(hs_cell, hs_digit) = state.findHiddenSingle(board);
    }
    if (hs_cell >= 0) {
        auto hs_cell_idx = static_cast<size_t>(hs_cell);
        size_t hs_row = SIMDConstraintState::cellRow(hs_cell_idx);
        size_t hs_col = SIMDConstraintState::cellCol(hs_cell_idx);

        // Save state for backtracking (including region tracking)
        std::array<uint16_t, 20> hs_saved_peer_candidates{};
        const auto& hs_peers = PEER_TABLE[hs_cell_idx];
        for (size_t i = 0; i < 20; ++i) {
            hs_saved_peer_candidates[i] = state.candidates[hs_peers.peers[i]];
        }
        uint16_t hs_saved_cell_candidates = state.getCandidates(hs_cell_idx);

        // Save region tracking state
        uint16_t saved_row_used = state.row_used_[hs_row];
        uint16_t saved_col_used = state.col_used_[hs_col];
        size_t hs_box = SIMDConstraintState::getBoxIndex(hs_row, hs_col);
        uint16_t saved_box_used = state.box_used_[hs_box];

        // Place the hidden single (forced move - no branching)
        board[hs_row][hs_col] = static_cast<int8_t>(hs_digit);
        state.place(hs_cell_idx, hs_digit);

        int count_before = count;
        // Incremental hash: XOR out empty cell, XOR in placed digit
        uint64_t hs_hash =
            hash ^ zobrist_table_[hs_row][hs_col][0] ^ zobrist_table_[hs_row][hs_col][static_cast<size_t>(hs_digit)];
        // Dirty regions: only the row, col, box of the placed cell
        uint32_t hs_dirty = (1U << hs_row) | (1U << (9 + hs_col)) | (1U << (18 + hs_box));
        countSolutionsHelperSIMD(board, state, count, max_solutions, start_time, timeout, timed_out, hs_hash,
                                 recursion_count, hs_dirty);

        // Backtrack: restore board
        board[hs_row][hs_col] = 0;

        // Backtrack: restore SIMD state
        state.candidates[hs_cell_idx] = hs_saved_cell_candidates;
        for (size_t i = 0; i < 20; ++i) {
            state.candidates[hs_peers.peers[i]] = hs_saved_peer_candidates[i];
        }

        // Backtrack: restore region tracking
        state.row_used_[hs_row] = saved_row_used;
        state.col_used_[hs_col] = saved_col_used;
        state.box_used_[hs_box] = saved_box_used;

        // Only cache if we fully explored the subtree (not truncated by max_solutions or timeout)
        if (count < max_solutions && !timed_out) {
            solution_count_cache_.insert(hash, count - count_before);
        }
        return;
    }

    // OPTIMIZATION: Naked singles propagation
    // Find cells with exactly 1 candidate and place them (no branching choice)
    int naked_single = state.findNakedSingle(board);
    if (naked_single >= 0) {
        auto ns_cell = static_cast<size_t>(naked_single);
        size_t ns_row = SIMDConstraintState::cellRow(ns_cell);
        size_t ns_col = SIMDConstraintState::cellCol(ns_cell);
        int ns_digit = state.getSolvedDigit(ns_cell);

        if (ns_digit == 0) {
            // Contradiction: cell has 0 candidates
            solution_count_cache_.insert(hash, 0);
            return;
        }

        // Save state for backtracking (including region tracking)
        std::array<uint16_t, 20> ns_saved_peer_candidates{};
        const auto& ns_peers = PEER_TABLE[ns_cell];
        for (size_t i = 0; i < 20; ++i) {
            ns_saved_peer_candidates[i] = state.candidates[ns_peers.peers[i]];
        }
        uint16_t ns_saved_cell_candidates = state.getCandidates(ns_cell);

        // Save region tracking state
        uint16_t saved_row_used = state.row_used_[ns_row];
        uint16_t saved_col_used = state.col_used_[ns_col];
        size_t ns_box = SIMDConstraintState::getBoxIndex(ns_row, ns_col);
        uint16_t saved_box_used = state.box_used_[ns_box];

        // Place the naked single (forced move)
        board[ns_row][ns_col] = static_cast<int8_t>(ns_digit);
        state.place(ns_cell, ns_digit);

        int count_before = count;
        // Incremental hash: XOR out empty cell, XOR in placed digit
        uint64_t ns_hash =
            hash ^ zobrist_table_[ns_row][ns_col][0] ^ zobrist_table_[ns_row][ns_col][static_cast<size_t>(ns_digit)];
        // Dirty regions: only the row, col, box of the placed cell
        uint32_t ns_dirty = (1U << ns_row) | (1U << (9 + ns_col)) | (1U << (18 + ns_box));
        countSolutionsHelperSIMD(board, state, count, max_solutions, start_time, timeout, timed_out, ns_hash,
                                 recursion_count, ns_dirty);

        // Backtrack: restore board
        board[ns_row][ns_col] = 0;

        // Backtrack: restore SIMD state
        state.candidates[ns_cell] = ns_saved_cell_candidates;
        for (size_t i = 0; i < 20; ++i) {
            state.candidates[ns_peers.peers[i]] = ns_saved_peer_candidates[i];
        }

        // Backtrack: restore region tracking
        state.row_used_[ns_row] = saved_row_used;
        state.col_used_[ns_col] = saved_col_used;
        state.box_used_[ns_box] = saved_box_used;

        // Only cache if we fully explored the subtree (not truncated by max_solutions or timeout)
        if (count < max_solutions && !timed_out) {
            solution_count_cache_.insert(hash, count - count_before);
        }
        return;
    }

    // SIMD MRV heuristic: Find cell with minimum candidates > 1
    int cell = state.findMRVCell();

    // No unsolved cells with > 1 candidates - all cells are solved!
    if (cell < 0) {
        // All cells are either filled on board OR have exactly 0 candidates (contradiction)
        // Since we handled naked singles above, findMRVCell returning -1 means
        // all unsolved cells have 0 candidates (contradiction) or board is complete
        bool complete = true;
        for (size_t row = 0; row < BOARD_SIZE && complete; ++row) {
            for (size_t col = 0; col < BOARD_SIZE && complete; ++col) {
                if (board[row][col] == 0) {
                    complete = false;
                }
            }
        }
        if (complete) {
            ++count;
            solution_count_cache_.insert(hash, 1);
        } else {
            // Contradiction - empty cell with no candidates
            solution_count_cache_.insert(hash, 0);
        }
        return;
    }

    // Check for contradiction at selected cell (no candidates)
    if (state.isContradiction(static_cast<size_t>(cell))) {
        solution_count_cache_.insert(hash, 0);
        return;
    }

    size_t row = SIMDConstraintState::cellRow(static_cast<size_t>(cell));
    size_t col = SIMDConstraintState::cellCol(static_cast<size_t>(cell));

    // Get candidate bitmask and iterate set bits (faster than 1-9 loop)
    uint16_t candidates = state.getCandidates(static_cast<size_t>(cell));

    // Save state for backtracking (including region tracking)
    std::array<uint16_t, 20> saved_peer_candidates{};
    const auto& peers = PEER_TABLE[static_cast<size_t>(cell)];
    for (size_t i = 0; i < 20; ++i) {
        saved_peer_candidates[i] = state.candidates[peers.peers[i]];
    }
    uint16_t saved_cell_candidates = candidates;

    // Save region tracking state
    uint16_t saved_row_used = state.row_used_[row];
    uint16_t saved_col_used = state.col_used_[col];
    size_t box = SIMDConstraintState::getBoxIndex(row, col);
    uint16_t saved_box_used = state.box_used_[box];

    int solutions_from_this_state = 0;

    // Iterate only valid candidates using CTZ (count trailing zeros)
    while (candidates != 0 && count < max_solutions) {
        // Extract lowest set bit (digit - 1)
        int bit_pos = std::countr_zero(static_cast<unsigned>(candidates));
        int digit = bit_pos + 1;

        // Clear lowest set bit for next iteration
        candidates &= candidates - 1;

        // Place digit
        board[row][col] = static_cast<int8_t>(digit);
        state.place(static_cast<size_t>(cell), digit);

        int count_before_recurse = count;

        // Recurse with incremental hash: XOR out empty cell, XOR in placed digit
        uint64_t new_hash = hash ^ zobrist_table_[row][col][0] ^ zobrist_table_[row][col][static_cast<size_t>(digit)];
        // Dirty regions: only the row, col, box of the placed cell
        uint32_t branch_dirty = (1U << row) | (1U << (9 + col)) | (1U << (18 + box));
        countSolutionsHelperSIMD(board, state, count, max_solutions, start_time, timeout, timed_out, new_hash,
                                 recursion_count, branch_dirty);

        solutions_from_this_state += (count - count_before_recurse);

        // Backtrack: restore board
        board[row][col] = 0;

        // Backtrack: restore SIMD state
        state.candidates[static_cast<size_t>(cell)] = saved_cell_candidates;
        for (size_t i = 0; i < 20; ++i) {
            state.candidates[peers.peers[i]] = saved_peer_candidates[i];
        }

        // Backtrack: restore region tracking
        state.row_used_[row] = saved_row_used;
        state.col_used_[col] = saved_col_used;
        state.box_used_[box] = saved_box_used;

        if (timed_out) {
            return;
        }
    }

    // Only cache if we fully explored all branches (not truncated by max_solutions or timeout)
    if (count < max_solutions && !timed_out) {
        solution_count_cache_.insert(hash, solutions_from_this_state);
    }
}

// ============================================================================
// Phase 3: Iterative Constraint Propagation
// ============================================================================

bool PuzzleGenerator::hasContradiction(const std::vector<std::vector<int>>& board) {
    ConstraintState state(board);

    // Check each empty cell for empty domain (no legal values)
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            if (board[row][col] == 0) {
                // Count possible values using ConstraintState
                int possible_count = state.countPossibleValues(row, col);

                // If no legal values, contradiction exists
                if (possible_count == 0) {
                    return true;
                }
            }
        }
    }

    return false;  // No contradiction found
}

std::expected<std::vector<std::vector<int>>, GenerationError>
// NOLINTNEXTLINE(readability-function-cognitive-complexity) — constraint propagation loop with naked/hidden single detection; algorithmically coupled, cannot split without losing context
PuzzleGenerator::propagateConstraintsScalar(const std::vector<std::vector<int>>& board) {
    auto propagated = board;
    ConstraintState state(propagated);

    bool changed = true;
    int iteration_count = 0;
    constexpr int MAX_ITERATIONS = 100;  // Prevent infinite loops

    while (changed && iteration_count < MAX_ITERATIONS) {
        changed = false;
        iteration_count++;

        // Try to find and place naked singles
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (propagated[row][col] == 0) {
                    int possible_count = state.countPossibleValues(row, col);

                    // Naked single: exactly one possible value
                    if (possible_count == 1) {
                        // Find which value it is
                        uint16_t mask = state.getPossibleValuesMask(row, col);
                        int value = 0;
                        for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
                            if ((mask & valueToBit(v)) != 0) {
                                value = v;
                                break;
                            }
                        }

                        propagated[row][col] = value;
                        state.placeValue(row, col, value);
                        changed = true;
                        spdlog::debug("Propagation: Naked single found at ({}, {}) = {}", row, col, value);
                    } else if (possible_count == 0) {
                        // Contradiction: no legal values
                        spdlog::debug("Propagation: Contradiction at ({}, {}) - empty domain", row, col);
                        return std::unexpected(GenerationError::NoUniqueSolution);
                    }
                }
            }
        }

        // Try to find and place hidden singles (only if no naked singles found)
        if (!changed) {
            auto hidden_opt = state.findHiddenSingle(propagated);
            if (hidden_opt.has_value()) {
                const auto& [pos, value] = hidden_opt.value();
                propagated[pos.row][pos.col] = value;
                state.placeValue(pos.row, pos.col, value);
                changed = true;
                spdlog::debug("Propagation: Hidden single found at ({}, {}) = {}", pos.row, pos.col, value);
            }
        }
    }

    if (iteration_count >= MAX_ITERATIONS) {
        spdlog::warn("Propagation: Max iterations ({}) reached", MAX_ITERATIONS);
    } else {
        spdlog::debug("Propagation: Fixed point reached after {} iterations", iteration_count);
    }

    return propagated;
}

std::expected<std::vector<std::vector<int>>, GenerationError>
PuzzleGenerator::propagateConstraintsSIMD(const std::vector<std::vector<int>>& board) {
    auto propagated = board;
    SIMDConstraintState state;
    state.initFromBoard(propagated);

    bool changed = true;
    int iteration_count = 0;
    constexpr int MAX_ITERATIONS = 100;

    while (changed && iteration_count < MAX_ITERATIONS) {
        changed = false;
        iteration_count++;

        // Try to find and place naked singles using SIMD
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (propagated[row][col] == 0) {
                    size_t cell_idx = (row * BOARD_SIZE) + col;
                    int candidate_count = state.countCandidates(cell_idx);

                    if (candidate_count == 1) {
                        // Naked single found
                        int value = state.getSolvedDigit(cell_idx);
                        propagated[row][col] = value;
                        state.place(cell_idx, value);
                        changed = true;
                        spdlog::debug("Propagation (SIMD): Naked single at ({}, {}) = {}", row, col, value);
                    } else if (candidate_count == 0) {
                        // Contradiction
                        spdlog::debug("Propagation (SIMD): Contradiction at ({}, {}) - empty domain", row, col);
                        return std::unexpected(GenerationError::NoUniqueSolution);
                    }
                }
            }
        }

        // Try hidden singles with SIMD (returns std::pair<int, int>, -1 means not found)
        if (!changed) {
            auto [cell_idx, value] = state.findHiddenSingle(propagated);
            if (cell_idx >= 0) {
                size_t row = static_cast<size_t>(cell_idx) / BOARD_SIZE;
                size_t col = static_cast<size_t>(cell_idx) % BOARD_SIZE;
                propagated[row][col] = value;
                state.place(static_cast<size_t>(cell_idx), value);
                changed = true;
                spdlog::debug("Propagation (SIMD): Hidden single at ({}, {}) = {}", row, col, value);
            }
        }
    }

    if (iteration_count >= MAX_ITERATIONS) {
        spdlog::warn("Propagation (SIMD): Max iterations ({}) reached", MAX_ITERATIONS);
    } else {
        spdlog::debug("Propagation (SIMD): Fixed point reached after {} iterations", iteration_count);
    }

    return propagated;
}

std::expected<std::vector<std::vector<int>>, GenerationError>
PuzzleGenerator::propagateConstraints(const std::vector<std::vector<int>>& board) {
    if (hasAvx2()) {
        return propagateConstraintsSIMD(board);
    }
    return propagateConstraintsScalar(board);
}

// ============================================================================
// Phase 3: Solver Integration - Iterative Propagation Helpers
// ============================================================================

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — iterative propagation loop; naked/hidden single passes are tightly coupled; complexity is inherent to constraint propagation
[[nodiscard]] bool PuzzleGenerator::applyIterativePropagationScalar(std::vector<std::vector<int>>& board,
                                                                    ConstraintState& state) {
    bool changed = true;
    int iteration_count = 0;
    constexpr int MAX_ITERATIONS = 81;  // Maximum possible iterations (9x9 board)

    while (changed && iteration_count < MAX_ITERATIONS) {
        changed = false;
        iteration_count++;

        // Try naked singles first (most efficient to find)
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] == 0) {
                    int possible_count = state.countPossibleValues(row, col);

                    if (possible_count == 1) {
                        // Extract the single value from bitmask
                        uint16_t mask = state.getPossibleValuesMask(row, col);
                        int value = 0;
                        for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
                            if ((mask & valueToBit(v)) != 0) {
                                value = v;
                                break;
                            }
                        }

                        board[row][col] = value;
                        state.placeValue(row, col, value);
                        changed = true;
                    } else if (possible_count == 0) {
                        // Contradiction detected
                        return false;
                    }
                }
            }
        }

        // Try hidden singles if no naked singles found
        if (!changed) {
            auto hidden_opt = state.findHiddenSingle(board);
            if (hidden_opt.has_value()) {
                const auto& [pos, value] = hidden_opt.value();
                board[pos.row][pos.col] = value;
                state.placeValue(pos.row, pos.col, value);
                changed = true;
            }
        }
    }

    // Check if board is complete
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            if (board[row][col] == 0) {
                return false;  // Not complete yet
            }
        }
    }

    return true;  // Board is complete!
}

[[nodiscard]] bool PuzzleGenerator::applyIterativePropagationSIMD(std::vector<std::vector<int>>& board,
                                                                  SIMDConstraintState& state) {
    bool changed = true;
    int iteration_count = 0;
    constexpr int MAX_ITERATIONS = 81;

    while (changed && iteration_count < MAX_ITERATIONS) {
        changed = false;
        iteration_count++;

        // Try hidden singles first (higher priority, more powerful)
        auto [hs_cell, hs_digit] = state.findHiddenSingle(board);
        if (hs_cell >= 0) {
            size_t hs_row = SIMDConstraintState::cellRow(static_cast<size_t>(hs_cell));
            size_t hs_col = SIMDConstraintState::cellCol(static_cast<size_t>(hs_cell));

            board[hs_row][hs_col] = hs_digit;
            state.place(static_cast<size_t>(hs_cell), hs_digit);
            changed = true;
            continue;  // Restart loop to find more forced moves
        }

        // Try naked singles
        int ns_cell = state.findNakedSingle(board);
        if (ns_cell >= 0) {
            size_t ns_row = SIMDConstraintState::cellRow(static_cast<size_t>(ns_cell));
            size_t ns_col = SIMDConstraintState::cellCol(static_cast<size_t>(ns_cell));
            int ns_digit = state.getSolvedDigit(static_cast<size_t>(ns_cell));

            if (ns_digit == 0) {
                // Contradiction: cell has 0 candidates
                return false;
            }

            board[ns_row][ns_col] = ns_digit;
            state.place(static_cast<size_t>(ns_cell), ns_digit);
            changed = true;
            continue;
        }
    }

    // Check if board is complete
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            if (board[row][col] == 0) {
                return false;  // Not complete yet
            }
        }
    }

    return true;  // Board is complete!
}

}  // namespace sudoku::core