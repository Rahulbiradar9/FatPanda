# Chess Engine

A clean, modular, and performance-oriented Chess Engine built from scratch in C++20.

## Project Structure

The project has a modular layout separating core chess logic, search, evaluation, hash tables, and communication protocols (UCI).

```text
Chess-engine/
├── CMakeLists.txt         # Main CMake build file
├── README.md              # Documentation
├── src/                   # Source files
│   ├── main.cpp           # Program entry point
│   ├── board/             # Board representation (Bitboards, state)
│   ├── move/              # Move generation, encoding, and ordering
│   ├── search/            # Search algorithms (Alpha-Beta, Iterative Deepening)
│   ├── evaluation/        # Evaluation function
│   ├── hash/              # Transposition tables & Zobrist hashing
│   ├── uci/               # Universal Chess Interface implementation
│   └── utils/             # Helper libraries and utilities
└── tests/                 # Unit tests (powered by GoogleTest)
```

## Requirements

* **C++20** compatible compiler (e.g., GCC 10+, Clang 10+, or MSVC 2019+)
* **CMake 3.14+**
* Cross-platform build tools (make, ninja, MSBuild, etc.)

## Building and Running

This project uses CMake to configure, build, and run tests.

### 1. Configure the build
Generate the build system files in the `build` directory:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

### 2. Build the engine
Compile the library, executable, and test suite:
```bash
cmake --build build --config Release
```

### 3. Run the engine
Run the compiled executable:
* **Windows (MSVC)**:
  ```cmd
  .\build\src\Release\ChessEngine.exe
  ```
* **Linux / macOS / Windows (Ninja/Make)**:
  ```bash
  ./build/src/ChessEngine
  ```

### 4. Run tests
Execute the unit tests using `ctest`:
```bash
cd build
ctest -C Release --output-on-failure
```

## License

This project is open-source and available under the MIT License.
