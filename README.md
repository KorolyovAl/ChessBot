# ChessBot (Qt & C++20)

Desktop chess application with a simple Qt Widgets UI and an embedded chess engine.  
The engine is built around a bitboard representation with legal move generation, evaluation (material + PST), and an alpha-beta search with common practical heuristics (move ordering, quiescence, SEE, transposition table).

> **Status:** 🚧 **Work in progress.**  
> The project is under development, APIs and behavior may change, and some features can be incomplete or unstable.


## Requirements
This project is built with qmake and requires:

- **Qt:** 5+ (Widgets module)
- **C++ standard:** C++20
- **Compiler with C++20 support:** GCC 10+ / Clang 11+ / MSVC (Visual Studio 2019 16.11+ or VS 2022)

## Build & Run

### Qt Creator (recommended)
1. Open `ChessBot/ChessBot.pro` in Qt Creator
2. Select your Kit
3. Build and Run

### Command line (Linux/macOS)
From the repository root:
```bash
cd ChessBot
qmake ChessBot.pro
make -j
./ChessBot
```

## Known Issues / Bugs / Limitations

### Missing / Incomplete
1. **No full game-end logic.**  
   There is no complete end-of-game handling yet (e.g., robust checkmate/stalemate/draw detection and proper game termination flow).
   **Priority:** medium.

2. **No smooth drag-and-drop animation.**  
   Piece movement is functional, but smooth dragging/animation is not implemented yet.  
   **Priority:** low.

## Features
- Qt GUI: board widget, click-to-move interaction
- Piece sprites via `.qrc` resources
- Core engine modules:
  - Bitboards, Zobrist hashing, repetition history
  - Legal move generation (precomputed masks + runtime generation)
  - Evaluation: piece values + PST tables
  - Search: iterative deepening + alpha-beta, PV line, quiescence
  - Move ordering + Static Exchange Evaluation (SEE)
  - Transposition Table (Zobrist key-based)

 
