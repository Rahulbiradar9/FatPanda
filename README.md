<div align="center">
  <img width="240" height="240" alt="FatPanda" src="logo.png" />
  <h1>FatPanda Chess Engine</h1>

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/Standard-C%2B%2B20-red.svg)](https://en.cppreference.com/w/cpp/20)
[![GitHub Release](https://img.shields.io/github/v/release/Rahulbiradar9/FatPanda?logo=github&color=097BBC)](https://github.com/Rahulbiradar9/FatPanda/releases/latest)
</div>

FatPanda is an open-source competitive chess engine built from scratch in C++20. Designed for performance, modularity, and tactical precision, it features an efficient bitboard representation, neural network (NNUE) and classical evaluation, advanced alpha-beta search heuristics, Syzygy endgame tablebase support, Polyglot opening books, and full UCI protocol compliance.

---

## Rating

| Version | [SPCC](https://www.sp-cc.de/) | [CCRL Blitz](https://www.computerchess.org.uk/ccrl/404/cgi/compare_engines.cgi?class=Single-CPU+engines&only_best_in_class=on) | [CCRL 40/15](https://www.computerchess.org.uk/ccrl/4040/cgi/compare_engines.cgi?class=Single-CPU+engines&only_best_in_class=on) | Release Date |
| :--- | :--- | :--- | :--- | :--- |
| [FatPanda v0.1.1](https://github.com/Rahulbiradar9/FatPanda/releases/tag/v0.1.1) | — | ~2060 (Est.) | — | Sep 26, 2026 |
| [FatPanda v0.1.0](https://github.com/Rahulbiradar9/FatPanda/releases/tag/v0.1.0) | — | ~2060 (Est.) | — | Aug 2, 2026 |

> [!NOTE]
> Rating lists (SPCC, CCRL Blitz, and CCRL 40/15) will be updated as official tournament testing and community rating submissions progress.

### Benchmark Matches & PGN Records

Calibration matches played in Cute Chess (100 games per tier, 300 games total) against calibrated Stockfish opponents:

| Match | Opponent Setting | Score (W - L - D) | Win% | Elo Difference | Performance Rating | Match PGN |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Match 1** | Stockfish **1800 Elo** | 51 – 41 – 8 | 55.0% | +35 | **~1,835 Elo** | [fatpanda_vs_stockfish1800-2.pgn](matches/fatpanda_vs_stockfish1800-2.pgn) |
| **Match 2** | Stockfish **2200 Elo** | 22 – 60 – 18 | 31.0% | -139.0 ± 66.5 | **~2,061 Elo** | [fatpanda_vs_stockfish2200-4.pgn](matches/fatpanda_vs_stockfish2200-4.pgn) |
| **Match 3** | Stockfish **2500 Elo** | 5 – 86 – 9 | 9.5% | -391.6 ± 109.4 | **~2,108 Elo** | [fatpanda_vs_stockfish2500-3.pgn](matches/fatpanda_vs_stockfish2500-3.pgn) |

---

## Getting Started

### Precompiled binaries

Precompiled binaries for official releases are available on the [GitHub Releases page](https://github.com/Rahulbiradar9/FatPanda/releases).

- **AVX2 / BMI2**: Fast, optimized for modern CPUs with AVX2/BMI2 instruction support (recommended).
- **Generic**: Portable build compatible with virtually all 64-bit CPUs, but slower than AVX2 builds.

> [!TIP]
> If you are unsure which binary to use, try the AVX2 build first. If it does not run on your system, fall back to the generic build.

---

### Building from source

To build FatPanda from source, ensure you have:

- A **C++20** compliant compiler:
  - GCC 10+
  - Clang 10+
  - MSVC 2019+ (Visual Studio 16.0 or newer)
- **CMake 3.14+**
- Build tools (Make, Ninja, or MSBuild)

#### Standard Build

```bash
# 1. Configure the build with Release optimizations
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 2. Build the engine executable
cmake --build build --config Release
```

The compiled binary will be located in:
- **Windows (MSVC)**: `.\build\src\Release\FatPanda.exe`
- **Linux / macOS / Ninja**: `./build/src/FatPanda`

#### Profile-Guided Optimization (PGO) builds

FatPanda includes built-in PGO support in CMake to maximize search speeds and NPS (Nodes Per Second):

```bash
# Step 1: Configure and build with profile generation instrumentation
cmake -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_PGO=GENERATE
cmake --build build --config Release

# Step 2: Run benchmark to collect execution profile data
./build/src/Release/FatPanda.exe --bench 13 1 16

# Step 3: Reconfigure and build with profile-guided optimization applied
cmake -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_PGO=APPLY
cmake --build build --config Release
```

---

### Running unit tests

FatPanda features a comprehensive test suite powered by GoogleTest covering move generation, search, evaluation, Syzygy tablebases, and UCI parsing:

```bash
cd build
ctest -C Release --output-on-failure
```

---

## Acknowledgements

- The [Chess Programming Wiki](https://www.chessprogramming.org/Main_Page) for its invaluable educational resources and algorithms.
- Open-source chess engines including [Stockfish](https://github.com/official-stockfish/Stockfish), [Reckless](https://github.com/codedeliveryservice/Reckless), [PlentyChess](https://github.com/Yoshie2000/PlentyChess), [Ethereal](https://github.com/AndyGrant/Ethereal), and [Berserk](https://github.com/jhonnold/berserk) for inspiring clean engine design patterns and heuristics.
- [Fathom](https://github.com/jdart1/Fathom) for Syzygy tablebase probing techniques.
- [Cute Chess](https://github.com/cutechess/cutechess) and [Fastchess](https://github.com/Disservin/fastchess) for match testing tools.
- Members of the computer chess community and [CCRL](https://www.computerchess.org.uk/ccrl/) testers.

---

## License

This project is licensed under the [MIT License](https://opensource.org/licenses/MIT).
