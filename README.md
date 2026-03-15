# Othello

---

Othello/Reversi game coded in C++. Can play against an 'AI'.

- Since you're here, I assume you already know how to play... If not, [here's a link](https://en.wikipedia.org/wiki/Reversi#Rules).
- To play the game,
  1. Run the `main` binary file and follow the prompts, or
  2. Download the source code and run `make` to compile, then run the `main` binary file.

**Disclaimer**: If you are on Windows then you may need to compile the code yourself; the provided Makefile and binary executable was meant for Unix-based systems.

---

## Game modes
1. Player vs Player
2. Player vs AI
3. AI vs AI
4. Puzzle Mode

---

## Features
- **Beautiful TUI** — Unicode board with box-drawing borders, a filled `●` for White and hollow `○` for Black, and valid moves highlighted in green.
- AI time limit can be set to any positive integer (seconds), **default = 5**. In AI vs AI mode each side has its own time limit.
- In PvE mode the player can choose their disk color and consequently, who starts first.
- **Undo** (`U`) — reverts your last move. In PvE mode, undoes back to the player's own last turn, skipping over the AI's move.
- **Resign** (`R`) — immediately concedes the game and awards the win to the opponent.
- **Game log export** — at the end of every game, two files are saved to the `games/` folder:
  - `.log` — raw move sequence on a single line
  - `.txt` — human-readable log with move history, scores, result, and game settings

---

## Puzzle Mode

Puzzle Mode lets you load a custom board position and watch the AI play both sides from that state.

**Setup**:
1. Choose a board input format:
   - **FEN-like string** — rows separated by `/`, using `B`/`b` (Black), `W`/`w` (White), and digits `1–8` for empty cells. Example: `8/8/8/3WB3/3BW3/8/8/8`
   - **64-character string** — left-to-right, top-to-bottom using `B`/`b`, `W`/`w`, and `.` for empty. Example: `...........................WB......BW...........................`
2. Specify whose turn it is (`B` for Black, `W` for White).
3. Set a time limit for the AI (shared by both sides).

The AI then plays out the game from the given position.

---

## Notes on 'AI'
- **Search**: Iterative deepening (IDDFS) with a time limit.
- **Move ordering**: Moves are tried in priority order (corners → edges → inner cells → X-squares) to improve pruning efficiency.
- **Evaluation components**:
  - *Material*: positional weights per square
  - *Mobility*: relative number of legal moves available
  - *Stability*: discs that can never be flipped (corners, filled edges, enclosed interior)
  - *Frontier*: discs adjacent to empty squares (fewer is better)
- **Dynamic danger correction**: static penalties for X/C-squares are lifted when the adjacent corner is already owned, since those squares are no longer a liability.
