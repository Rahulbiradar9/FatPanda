<div align="center">
  <img width="240" height="240" alt="FatPanda" src="logo.png" />
  <h1>FatPanda</h1>

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![GitHub Release](https://img.shields.io/github/v/release/Rahulbiradar9/FatPanda?logo=github&color=097BBC)](https://github.com/Rahulbiradar9/FatPanda/releases/latest)
</div>

FatPanda is a clean, modular, and performance-oriented competitive chess engine built from scratch in C++20. It implements efficient bitboard representations, legal move generation, transposition tables with Zobrist hashing, and a highly tuned search-evaluation architecture.

## Releases

| Version | Description | Release Date |
| --- | --- | --- |
| [FatPanda v0.1.0][v0.1.0] | First stable release with Bitboard movegen, Alpha-Beta search, transposition tables, and classical evaluation. | Aug 2, 2026 |

[v0.1.0]: https://github.com/Rahulbiradar9/FatPanda/releases/tag/v0.1.0

## Features

- **Board Representation**: Efficient Bitboard architecture with precalculated attack tables, custom FEN parsing, and board visualization.
- **Search Techniques**:
  - Negamax search with Alpha-Beta pruning
  - Iterative Deepening
  - Quiescence Search to avoid the horizon effect
  - Transposition Tables (TT) for caching search results
- **Move Ordering & Pruning**:
  - MVV-LVA (Most Valuable Victim - Least Valuable Aggressor) ordering
  - Killer Move heuristic
  - History heuristic
  - Principal Variation (PV) ordering
- **Evaluation**: Custom classical evaluation function assessing:
  - Material balance
  - Piece-Square Tables (PST) for positional play
  - Pawn structures (passed, isolated, doubled, and backward pawns)
  - King safety and piece mobility
- **Interface**: Full support for the Universal Chess Interface (UCI) protocol.

## Getting started

### Precompiled binaries

Precompiled binaries will be available on the [GitHub Releases page](https://github.com/Rahulbiradar9/FatPanda/releases) in future releases.

### Building from source

To build FatPanda from source, make sure you have:

- A **C++20** compatible compiler (e.g., GCC 10+, Clang 10+, or MSVC 2019+)
- **CMake 3.14+**
- Build tools (Make, Ninja, MSBuild, etc.)

Once installed, you can build FatPanda using CMake:

```bash
# 1. Configure the build
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 2. Build the engine
cmake --build build --config Release
```

The compiled executable will be located in:
- **Windows (MSVC)**: `.\build\src\Release\FatPanda.exe`
- **Linux / macOS / Windows (Ninja/Make)**: `./build/src/FatPanda`

### Running unit tests

FatPanda includes a comprehensive test suite powered by GoogleTest. To execute all unit tests, run:

```bash
cd build
ctest -C Release --output-on-failure
```

### Usage

FatPanda is a backend chess engine communicating via the standard UCI protocol. It is designed to be used with UCI-compatible Graphical User Interfaces (GUIs), such as:
- [Cute Chess](https://github.com/cutechess/cutechess)
- [En Croissant](https://encroissant.org)
- [Nibbler](https://github.com/rooklift/nibbler)

Alternatively, you can interact with the engine directly through the command line or run its demo verification mode.

#### Command-line arguments
- `demo` / `--demo`: Run a demo verification checking the move representation, move generator, static evaluator, FEN parser, and starting board layout.

```bash
# Example running the demo mode
./build/src/FatPanda --demo
```

### UCI options

FatPanda supports the following UCI options:

| Option | Default | Description |
| --- | --- | --- |
| Hash | 64 | Size of the transposition table in MB [1–2048] |

### Custom UCI commands

Along with the standard UCI commands (like `position`, `go`, `stop`, `ucinewgame`), FatPanda supports:

| Command | Description |
| --- | --- |
| `print` or `d` | Print the current board position and active FEN in a human-readable format |

## Acknowledgements

- The [Chess Programming Wiki](https://www.chessprogramming.org/Main_Page) for its invaluable resources on chess programming concepts.
- Open-source engines like [Stockfish](https://github.com/official-stockfish/Stockfish) and [Reckless](https://github.com/codedeliveryservice/Reckless) for inspiring design patterns and clean architectures.

## License

This project is licensed under the [MIT License](https://opensource.org/licenses/MIT).
