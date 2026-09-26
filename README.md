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

## Usage

FatPanda is a backend chess engine communicating via the standard **Universal Chess Interface (UCI)** protocol. It is designed to be used with UCI-compatible Graphical User Interfaces (GUIs), such as:

- [Cute Chess](https://github.com/cutechess/cutechess)
- [En Croissant](https://encroissant.org/)
- [Nibbler](https://github.com/rooklift/nibbler)

*(See [`CuteChess_Testing_Guide_FatPanda_vs_Stockfish.docx`](CuteChess_Testing_Guide_FatPanda_vs_Stockfish.docx) for match configuration instructions).*

---

### UCI Options

FatPanda supports the following UCI configuration options:

| Name | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `Hash` | spin | 64 | Size of the transposition table in megabytes [1–2048] |
| `Clear Hash` | button | — | Clear the transposition table contents |
| `Threads` | spin | 1 | Number of search threads [1–128] |
| `MultiPV` | spin | 1 | Number of principal variations to calculate and output [1–256] |
| `Move Overhead` | spin | 20 | Time buffer in ms reserved to prevent flagging on time [0–5000] |
| `UCI_Chess960` | check | false | Enable Chess960 (Fischer Random Chess) support |
| `Ponder` | check | true | Enable pondering (calculating while waiting for opponent's move) |
| `OwnBook` | check | true | Enable built-in Polyglot opening book probing |
| `BookFile` | string | `book.bin` | Path to the Polyglot opening book file (`.bin`) |
| `SyzygyPath` | string | `<empty>` | Path to Syzygy endgame tablebase directory (`.rtbw` / `.rtbz`) |
| `SyzygyUse` | check | false | Enable Syzygy endgame tablebase probing |
| `Use NNUE` | check | false | Enable NNUE neural network evaluation |
| `EvalFile` | string | `nn.nnue` | Path to external NNUE evaluation weight file |
| `SingularExtension` | check | true | Enable singular move extensions |
| `SingularMargin` | spin | 2 | Margin for singular extension verification [0–100] |
| `IIR` | check | true | Enable Internal Iterative Reduction |
| `LMP` | check | true | Enable Late Move Pruning |
| `LMPMaxDepth` | spin | 8 | Maximum depth threshold for Late Move Pruning [1–16] |
| `ProbCut` | check | true | Enable ProbCut forward pruning |
| `ProbCutMargin` | spin | 100 | Margin for ProbCut pruning [10–500] |
| `CorrectionHistory` | check | true | Enable correction history adjustment to evaluation |
| `DeltaMargin` | spin | 200 | Margin for delta pruning in quiescence search [0–1000] |
| `Seed` | spin | 42 | Random seed for deterministic reproducibility |

---

### Custom Commands

Along with standard UCI commands (`uci`, `isready`, `ucinewgame`, `position`, `go`, `stop`, `ponderhit`, `quit`), FatPanda supports custom interactive commands:

| Command | Description |
| :--- | :--- |
| `bench [depth] [threads] [hash]` | Run the benchmark across standard positions to measure nodes and speed (NPS) |
| `d` or `print` | Display the current board in ASCII format alongside the active FEN |
| `epd <filepath> [movetime_ms]` | Run an EPD tactical test suite to test accuracy and solve rate |
| `tune export` | Export the current evaluation parameters |
| `tune import <params>` | Load custom evaluation parameters |
| `tune selfplay [games]` | Run an automated self-play parameter tuning match |
| `datagen [games] [depth] [output]` | Generate self-play training data records for NNUE training |

#### Command-Line Arguments (CLI)

You can also pass arguments directly when launching the binary:

```bash
# Run benchmark directly (default: depth 13, 1 thread, 16MB hash)
./FatPanda --bench 13 1 16

# Run self-verification diagnostic check
./FatPanda --demo

# Set specific random seed
./FatPanda --seed 12345

# Print version and git commit hash
./FatPanda --version
```

---

## Features Overview

- **Board Representation**:
  - High-performance Bitboard representation
  - Precalculated magic attack bitboards for sliding and non-sliding pieces
  - Zobrist hashing for fast transposition lookups and repetition detection
  - Robust FEN parser with full Chess960 support
  - Polyglot opening book parser
  - Syzygy 3-4-5-6 piece endgame tablebase probing

- **Search Architecture**:
  - Negamax search with Principal Variation Search (PVS) & Alpha-Beta pruning
  - Iterative Deepening with aspiration windows
  - Quiescence Search with tactical delta pruning
  - Transposition Table (TT) with depth-preferred replacement scheme
  - Lazy SMP multi-threading support

- **Heuristics & Pruning**:
  - Singular Extensions
  - Late Move Reductions (LMR) & Late Move Pruning (LMP)
  - Null Move Pruning (NMP)
  - ProbCut forward pruning
  - Internal Iterative Reduction (IIR)
  - Move Ordering: Hash move, MVV-LVA, Killer heuristic, Countermove heuristic, History heuristic, Correction History

- **Evaluation**:
  - Dual-perspective Efficiently Updatable Neural Network (NNUE) evaluation
  - Hand-crafted classical evaluation (Material balance, Piece-Square Tables, Pawn structure analysis, King safety, Mobility)

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
