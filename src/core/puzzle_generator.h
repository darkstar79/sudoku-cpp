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

#include "game_validator.h"
#include "i_puzzle_generator.h"
#include "i_puzzle_rater.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>

namespace sudoku::core {

/// Fixed-size open-addressing hash table for Zobrist memoization cache.
/// Zero heap allocation, power-of-2 sizing with Fibonacci hashing.
/// Replaces std::unordered_map to eliminate per-insert malloc overhead.
class FlatSolutionCache {
public:
    /// Table capacity (must be power of 2). 2^13 = 8192 entries × 16 bytes = 128 KB.
    /// Sized for L2 cache — 4x average utilization (~2K entries), 32x smaller than original 4 MB.
    static constexpr size_t CAPACITY = 8192;
    static constexpr size_t MASK = CAPACITY - 1;

    /// Fibonacci hashing constant for 64-bit keys (golden ratio × 2^64)
    static constexpr uint64_t FIBONACCI_HASH = 11400714819323198485ULL;

    /// Sentinel key indicating an empty slot
    static constexpr uint64_t EMPTY_KEY = 0;

    struct Entry {
        uint64_t key{EMPTY_KEY};  ///< Zobrist hash (0 = empty slot)
        int value{0};             ///< Cached solution count
    };

    FlatSolutionCache() = default;

    /// Look up a key. Returns pointer to value if found, nullptr if not.
    [[nodiscard]] const int* find(uint64_t key) const {
        if (key == EMPTY_KEY) {
            return nullptr;  // Sentinel key cannot be stored
        }
        size_t idx = fibHash(key);
        for (size_t probe = 0; probe < MAX_PROBE; ++probe) {
            const auto& entry = table_[(idx + probe) & MASK];
            if (entry.key == key) {
                return &entry.value;
            }
            if (entry.key == EMPTY_KEY) {
                return nullptr;  // Empty slot = key not in table
            }
        }
        return nullptr;  // Probe limit reached
    }

    /// Insert or update a key-value pair. Silently drops if table is too full.
    void insert(uint64_t key, int value) {
        if (key == EMPTY_KEY || size_ >= MAX_LOAD) {
            return;  // Can't store sentinel key or table too full
        }
        size_t idx = fibHash(key);
        for (size_t probe = 0; probe < MAX_PROBE; ++probe) {
            auto& entry = table_[(idx + probe) & MASK];
            if (entry.key == EMPTY_KEY) {
                entry.key = key;
                entry.value = value;
                ++size_;
                return;
            }
            if (entry.key == key) {
                entry.value = value;  // Update existing
                return;
            }
        }
        // Probe limit reached — silently drop
    }

    /// Clear all entries
    void clear() {
        table_.fill(Entry{});
        size_ = 0;
    }

    [[nodiscard]] size_t size() const {
        return size_;
    }

private:
    [[nodiscard]] static size_t fibHash(uint64_t key) {
        return static_cast<size_t>((key * FIBONACCI_HASH) >> (64 - 13));  // 13 = log2(CAPACITY)
    }

    /// Max probe distance before giving up (keeps worst-case bounded)
    static constexpr size_t MAX_PROBE = 64;

    /// Max entries before we stop inserting (75% load factor)
    static constexpr size_t MAX_LOAD = (CAPACITY * 3) / 4;  // 6144

    std::array<Entry, CAPACITY> table_{};
    size_t size_{0};
};

// Forward declarations
struct Board;
class ConstraintState;
struct SIMDConstraintState;

/// Analysis of a clue's constraint dependencies
/// Used for intelligent clue selection in Phase 2 optimization
struct ClueAnalysis {
    Position pos{};            ///< Position of the clue
    int value{0};              ///< Value at this position
    int alternative_count{0};  ///< Number of alternative values if clue removed
    int constraint_score{0};   ///< Higher score = more constrained (9 - alternative_count)
};

/// Concrete implementation of Sudoku puzzle generator
class PuzzleGenerator : public IPuzzleGenerator {
public:
    /// Constructor without rating (backward compatible)
    PuzzleGenerator();

    /// Constructor with rating validation
    /// @param rater Puzzle rater for validating difficulty
    explicit PuzzleGenerator(std::shared_ptr<IPuzzleRater> rater);

    ~PuzzleGenerator() override = default;
    PuzzleGenerator(const PuzzleGenerator&) = delete;
    PuzzleGenerator& operator=(const PuzzleGenerator&) = delete;
    PuzzleGenerator(PuzzleGenerator&&) = delete;
    PuzzleGenerator& operator=(PuzzleGenerator&&) = delete;

    /// Generates a new puzzle with specified settings
    std::expected<Puzzle, GenerationError> generatePuzzle(const GenerationSettings& settings) const override;

    /// Generates a puzzle with default settings for given difficulty
    std::expected<Puzzle, GenerationError> generatePuzzle(Difficulty difficulty) const override;

    /// Solves a given puzzle and returns the solution
    std::expected<std::vector<std::vector<int>>, GenerationError>
    solvePuzzle(const std::vector<std::vector<int>>& board) const override;

    /// Checks if a puzzle has a unique solution
    bool hasUniqueSolution(const std::vector<std::vector<int>>& board) const override;

    /// Counts the number of clues (filled cells) in a puzzle
    int countClues(const std::vector<std::vector<int>>& board) const override;

    /// Validates that a puzzle is properly formed
    bool validatePuzzle(const std::vector<std::vector<int>>& board) const override;

    // Phase 1: Iterative Deepening Methods (for Expert puzzle generation)

    /// Attempts to remove clues from a solution to reach an exact target clue count
    /// Uses iterative approach: aggressive removal followed by fine-tuning
    /// @param solution Complete Sudoku solution
    /// @param target_clues Exact number of clues to achieve
    /// @param max_attempts Maximum attempts before giving up
    /// @param rng Random number generator
    /// @return Puzzle with target_clues if successful, empty board otherwise
    [[nodiscard]] std::vector<std::vector<int>> removeCluesToTarget(const std::vector<std::vector<int>>& solution,
                                                                    int target_clues, int max_attempts,
                                                                    std::mt19937& rng) const;

    /// Removes clues using iterative deepening strategy (Expert difficulty only)
    /// Targets specific clue counts sequentially (21→20→19→18→17) instead of greedy removal
    /// Falls back to standard greedy algorithm for Easy/Medium/Hard
    /// @param solution Complete Sudoku solution
    /// @param settings Generation settings (difficulty, seed, etc.)
    /// @param rng Random number generator
    /// @return Puzzle with clues in target range, or empty board if failed
    [[nodiscard]] std::vector<std::vector<int>>
    removeCluesToCreatePuzzleIterative(const std::vector<std::vector<int>>& solution,
                                       const GenerationSettings& settings, std::mt19937& rng) const;

    /// Runs one clue removal ordering on a copy of the solution.
    /// Shuffles positions and greedily removes clues until min_clues or stuck.
    /// @return Final clue count after removal
    [[nodiscard]] int runRemovalOrdering(std::vector<std::vector<int>>& puzzle,
                                         const std::vector<std::pair<size_t, size_t>>& positions, bool ensure_unique,
                                         int min_clues, int max_clues) const;

    // Phase 2: Intelligent Clue Dropping Methods (for improved re-completion)

    /// Analyzes constraint dependencies for all clues on the board
    /// Computes alternative_count (how many values could replace this clue)
    /// and constraint_score (9 - alternative_count, higher = more constrained)
    /// @param board Current puzzle board (may be partially filled)
    /// @return Vector of ClueAnalysis for all non-zero clues, sorted by constraint score
    [[nodiscard]] static std::vector<ClueAnalysis> analyzeClueConstraints(const std::vector<std::vector<int>>& board);

    /// Selects clues to drop during re-completion phase
    /// Prioritizes highly constrained clues (fewer alternatives) for better exploration
    /// @param board Current puzzle board
    /// @param num_clues Number of clues to select for dropping
    /// @param rng Random number generator
    /// @return Positions of selected clues to drop
    [[nodiscard]] static std::vector<Position> selectCluesForDropping(const std::vector<std::vector<int>>& board,
                                                                      int num_clues, std::mt19937& rng);

    // Phase 3: Iterative Constraint Propagation Methods (for faster solving)

    /// Applies iterative constraint propagation until fixed point
    /// Repeatedly applies naked singles and hidden singles until no more forced moves
    /// @param board Current board state
    /// @return Propagated board or error if contradiction detected
    [[nodiscard]] static std::expected<std::vector<std::vector<int>>, GenerationError>
    propagateConstraints(const std::vector<std::vector<int>>& board);

    /// Checks if board has any contradictions (empty domains)
    /// @param board Current board state
    /// @return true if contradiction exists, false otherwise
    [[nodiscard]] static bool hasContradiction(const std::vector<std::vector<int>>& board);

    /// Scalar version of constraint propagation (for testing)
    /// @param board Current board state
    /// @return Propagated board or error if contradiction detected
    [[nodiscard]] static std::expected<std::vector<std::vector<int>>, GenerationError>
    propagateConstraintsScalar(const std::vector<std::vector<int>>& board);

    /// SIMD version of constraint propagation (for testing)
    /// @param board Current board state
    /// @return Propagated board or error if contradiction detected
    [[nodiscard]] static std::expected<std::vector<std::vector<int>>, GenerationError>
    propagateConstraintsSIMD(const std::vector<std::vector<int>>& board);

    /// Set solver path override for benchmarking (persists across calls until changed)
    void setSolverPath(SolverPath path) {
        active_solver_path_ = path;
    }

    /// Get the currently active solver path
    [[nodiscard]] SolverPath solverPath() const {
        return active_solver_path_;
    }

private:
    GameValidator validator_;
    std::shared_ptr<IPuzzleRater> rater_;                      // Optional rater for difficulty validation
    mutable SolverPath active_solver_path_{SolverPath::Auto};  // Solver path for benchmarking

    /// Zobrist hash table for board state hashing (initialized once)
    mutable std::array<std::array<std::array<uint64_t, 10>, 9>, 9> zobrist_table_{};

    /// Memoization cache for solution counts (cleared per generation attempt)
    /// Uses FlatSolutionCache (128 KB fixed, zero heap allocation) instead of std::unordered_map
    mutable FlatSolutionCache solution_count_cache_;

    /// Generates a complete valid Sudoku solution
    std::vector<std::vector<int>> generateCompleteSolution(std::mt19937& rng) const;

    /// Removes clues from a complete solution to create a puzzle
    std::vector<std::vector<int>> removeCluesToCreatePuzzle(const std::vector<std::vector<int>>& solution,
                                                            const GenerationSettings& settings,
                                                            std::mt19937& rng) const;

    /// Expert-only re-completion phase: drops clues, re-solves, retries removal orderings
    /// Updates best_puzzle and best_clue_count in-place; returns early if target range is hit
    void runRecompletionPhase(std::vector<std::vector<int>>& best_puzzle, int& best_clue_count,
                              std::vector<std::pair<size_t, size_t>>& positions, bool ensure_unique, int min_clues,
                              int max_clues, int num_orderings, std::mt19937& rng) const;

    /// Solves a puzzle using backtracking algorithm
    bool solvePuzzleBacktrack(std::vector<std::vector<int>>& board) const;

    /// Counts the number of solutions for a given puzzle
    int countSolutions(const std::vector<std::vector<int>>& board, int max_solutions = 2) const;

    /// Counts the number of solutions with timeout protection
    int countSolutionsWithTimeout(const std::vector<std::vector<int>>& board, int max_solutions,
                                  std::chrono::milliseconds timeout) const;

    /// Helper for counting solutions with backtracking
    /// @param hash Zobrist hash of current board state (passed by value for automatic backtracking)
    /// @param recursion_count Shared counter for timeout check sampling (checked every 1024 calls)
    void countSolutionsHelper(Board& board, ConstraintState& state, int& count, int max_solutions,
                              const std::chrono::steady_clock::time_point& start_time,
                              const std::chrono::milliseconds& timeout, bool& timed_out, uint64_t hash,
                              uint32_t& recursion_count) const;

    /// SIMD-optimized helper for counting solutions (AVX2)
    /// Uses SIMDConstraintState for faster MRV heuristic and candidate enumeration
    /// @param hash Zobrist hash of current board state (passed by value for automatic backtracking)
    /// @param recursion_count Shared counter for timeout check sampling (checked every 1024 calls)
    /// @param dirty_regions Bitmask of regions to scan for hidden singles (bits 0-8=rows, 9-17=cols, 18-26=boxes)
    void countSolutionsHelperSIMD(Board& board, SIMDConstraintState& state, int& count, int max_solutions,
                                  const std::chrono::steady_clock::time_point& start_time,
                                  const std::chrono::milliseconds& timeout, bool& timed_out, uint64_t hash,
                                  uint32_t& recursion_count, uint32_t dirty_regions) const;

    /// Phase 3: Apply iterative propagation to fill all forced moves (SIMD version)
    /// Returns true if board is complete after propagation, false otherwise
    /// Modifies board and state in-place
    [[nodiscard]] static bool applyIterativePropagationSIMD(std::vector<std::vector<int>>& board,
                                                            SIMDConstraintState& state);

    /// Phase 3: Apply iterative propagation to fill all forced moves (scalar version)
    /// Returns true if board is complete after propagation, false otherwise
    /// Modifies board and state in-place
    [[nodiscard]] static bool applyIterativePropagationScalar(std::vector<std::vector<int>>& board,
                                                              ConstraintState& state);

    /// Fills the board with a valid complete solution using backtracking
    bool fillBoardRecursively(std::vector<std::vector<int>>& board, std::mt19937& rng) const;

    /// Gets target clue count range for difficulty level
    static std::pair<int, int> getClueRange(Difficulty difficulty);

    /// Finds next empty position in board
    /// @param board Current board state
    /// @param use_mcv_heuristic If true, use Most Constrained Variable heuristic
    ///                          (selects cell with fewest legal values for optimal pruning)
    /// @return Position of empty cell, or nullopt if board is full
    std::optional<Position> findEmptyPosition(const std::vector<std::vector<int>>& board,
                                              bool use_mcv_heuristic = false) const;

    /// Board overload for findEmptyPosition (flat iteration, delegates to vector version for MCV)
    std::optional<Position> findEmptyPosition(const Board& board, bool use_mcv_heuristic = false) const;

    /// Gets randomized list of numbers 1-9
    static std::vector<int> getShuffledNumbers(std::mt19937& rng);

    /// Creates empty 9x9 board
    static std::vector<std::vector<int>> createEmptyBoard();

    /// Initializes Zobrist hash table with random values
    void initializeZobristTable();

    /// Computes Zobrist hash for a board state
    [[nodiscard]] uint64_t computeBoardHash(const Board& board) const;

    /// Clears the memoization cache (called at start of each generation attempt)
    void clearCache() const;

    /// Resolve Auto solver path to a concrete path based on CPU features
    [[nodiscard]] SolverPath resolveEffectivePath() const;
};

}  // namespace sudoku::core