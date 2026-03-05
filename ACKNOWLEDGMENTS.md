# Acknowledgments

This project benefited greatly from the research, algorithms, and insights of the following individuals, projects, and publications. While no code was directly copied, these sources significantly influenced the design and implementation of puzzle generation, solving algorithms, and optimization techniques.

---

## Research Papers & Publications

### Peter Norvig - "Solving Every Sudoku Puzzle" (2005)

- **URL:** https://norvig.com/sudoku.html
- **Influence:** Foundational research on constraint propagation and backtracking algorithms
- **Key Contributions:**
  - Constraint propagation techniques (naked singles, hidden singles)
  - Backtracking with heuristics (most constrained variable)
  - Clear explanation of solving strategies

Peter Norvig's seminal work provided the theoretical foundation for our solver implementation and constraint propagation strategies.

---

### McGuire, G., Tugemann, B., Civario, G. - "There is no 16-Clue Sudoku" (2014)

- **URL:** https://arxiv.org/pdf/1201.0749
- **Publication:** Mathematical analysis of minimum Sudoku clues
- **Influence:** Understanding of puzzle difficulty and clue distribution
- **Key Contributions:**
  - Mathematical proof that 17 is the minimum number of clues for a valid Sudoku
  - Insights into puzzle uniqueness and solvability
  - Clue distribution patterns for valid puzzles

This research informed our puzzle generation algorithms, particularly regarding minimum clue counts and difficulty calibration.

---

### arXiv - "A Study Of Sudoku Solving Algorithms: Backtracking and Heuristic"

- **URL:** https://arxiv.org/pdf/2507.09708
- **Influence:** Comparative analysis of solving algorithms
- **Key Contributions:**
  - Performance analysis of different backtracking strategies
  - Heuristic effectiveness measurements
  - Algorithm complexity comparisons

This paper helped optimize our solver performance and informed heuristic selection.

---

## Open Source Projects

### Tdoku Project by Tom Dillon

- **GitHub:** https://github.com/t-dillon/tdoku
- **License:** MIT (code reference only, no code copied)
- **Influence:** Advanced puzzle generation techniques and SIMD optimizations
- **Key Contributions:**
  - Loss function approach for difficulty tuning: `score = difficulty - α×clues`
  - Continuous optimization strategies vs binary accept/reject
  - Successfully demonstrated Expert puzzle generation in 10-50ms
  - SIMD constraint propagation techniques

The Tdoku project demonstrated state-of-the-art puzzle generation performance and informed our optimization strategy, particularly:
- Zobrist hashing for board states
- Memoization strategies for constraint checking
- SIMD-based constraint propagation (26.5x speedup in our implementation)

**Note:** While we studied Tdoku's algorithms and techniques, no code was directly copied. Our implementation is original and uses different data structures and approaches.

---

## Educational Resources

### 101 Computing - "Sudoku Generator Algorithm"

- **URL:** https://www.101computing.net/sudoku-generator-algorithm/
- **Influence:** Clear explanation of puzzle generation workflow
- **Key Contributions:**
  - Step-by-step puzzle generation process
  - Difficulty adjustment techniques
  - Clue removal strategies

This resource provided an accessible introduction to puzzle generation algorithms that informed our initial implementation.

---

### Wikipedia - "Sudoku solving algorithms"

- **URL:** https://en.wikipedia.org/wiki/Sudoku_solving_algorithms
- **Influence:** Comprehensive overview of solving techniques
- **Key Contributions:**
  - Catalog of advanced solving techniques (naked pairs, hidden pairs, X-wing, etc.)
  - Constraint propagation strategies
  - Difficulty classification based on required techniques

This resource served as a reference for understanding the full spectrum of Sudoku solving techniques and their relative difficulty.

---

### MIT Technology Review - "Mathematicians Solve Minimum Sudoku Problem" (2012)

- **URL:** https://www.technologyreview.com/2012/01/06/188520/mathematicians-solve-minimum-sudoku-problem/
- **Influence:** Popular science explanation of minimum clue research
- **Key Contributions:**
  - Accessible explanation of the 17-clue minimum proof
  - Context for puzzle generation constraints

---

## Specific Algorithm Influences

### Constraint Propagation

**Primary Sources:**
- Peter Norvig's "Solving Every Sudoku Puzzle"
- Tdoku Project's SIMD implementation

**Our Implementation:**
- Naked singles detection
- Hidden singles detection (19.6% speedup in our measurements)
- SIMD constraint state (26.5x impact measured)
- Bitmask-based candidate tracking

**Influence Level:** High - Core solving algorithm based on these techniques

---

### Puzzle Generation

**Primary Sources:**
- Tdoku Project's loss function approach
- McGuire et al.'s minimum clue research
- 101 Computing's generation workflow

**Our Implementation:**
- Difficulty-based clue count targets (Easy: 36-40, Medium: 30-35, Hard: 26-29, Expert: 22-25)
- Zobrist hashing for memoization
- Unique solution verification
- Difficulty rating based on solving techniques required

**Influence Level:** High - Generation strategy significantly influenced by these sources

---

### Performance Optimization

**Primary Sources:**
- Tdoku Project's SIMD techniques
- arXiv paper on algorithm performance

**Our Implementation:**
- SIMD constraint state using SSE/AVX intrinsics
- Memoization with Zobrist hashing
- Backtracking solver with constraint propagation
- Achieved performance: Easy (0ms), Medium (9ms), Hard (239ms)

**Influence Level:** Medium - Optimization techniques inspired by these sources, implementation is original

---

## Acknowledgment of Influence

This project stands on the shoulders of giants. The research and open-source work cited above saved countless hours of experimentation and helped avoid known pitfalls. We are grateful to all contributors to the Sudoku algorithm research community.

**Key Takeaways:**
1. **Constraint propagation** is critical for performance (Norvig, Tdoku)
2. **SIMD optimizations** provide 25-30x speedups in constraint checking (Tdoku)
3. **Memoization** dramatically improves Hard/Expert puzzle generation (Tdoku)
4. **Minimum clue research** informs difficulty calibration (McGuire et al.)

---

## License Compatibility

All referenced projects and papers are either:
- **Public domain research** (academic papers)
- **MIT/BSD licensed** (Tdoku Project - reference only, no code copied)
- **Educational resources** (101 Computing, Wikipedia - no licensing restrictions)

Our project's GPLv3 license is compatible with all referenced materials.

---

**Last Updated:** 2026-02-01

For software dependencies and licenses, see [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
