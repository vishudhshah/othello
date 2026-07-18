# Othello

---

Othello/Reversi game coded in C++, with an ncurses TUI. Can play against an 'AI'.

- Since you're here, I assume you already know how to play... If not, [here's a link](https://en.wikipedia.org/wiki/Reversi#Rules).
- To play the game,
  1. Run the `main` binary file and follow the prompts, or
  2. Download the source code and run `make` to compile, then run the `main` binary file.

**Dependencies**: building from source requires ncurses (wide-char build). On macOS: `brew install ncurses`. The Makefile picks it up automatically via `brew --prefix ncurses`.

**Disclaimer**: If you are on Windows then you may need to compile the code yourself; the provided Makefile and binary executable was meant for Unix-based systems.

---

## Game modes
1. Player vs Player
2. Player vs AI
3. AI vs AI
4. Puzzle Mode

---

## Features
- **Interactive ncurses TUI** — the whole game (menus included) runs as a persistently redrawn curses screen: Unicode board with box-drawing borders, a filled `●` for White and hollow `○` for Black, valid moves highlighted in green, and the last move played highlighted in red. Falls back to plain ASCII glyphs and/or no color automatically on terminals that don't support Unicode or color.
- **Mouse or keyboard, your choice** — click any cell to play it, or type the coordinate (e.g. `D3`) and press Enter. Clicking near a cell's edge is forgiving; a click that lands exactly on the line between two cells is safely rejected (with a message) rather than guessing which one you meant.
- **Menus are navigable by arrow keys, `j`/`k` (vim-style), mouse click, or number keys** — game mode, time limits, disk color, and Puzzle Mode setup are all on-screen selection/entry screens rather than typed prompts.
- AI time limit can be set to any positive integer (seconds), **default = 5**. In AI vs AI mode each side has its own time limit.
- In PvE mode the player can choose their disk color and consequently, who starts first.
- **Undo** (`U`) — reverts your last move. In PvE mode, undoes back to the player's own last turn, skipping over the AI's move.
- **Resign** (`R`) — immediately concedes the game and awards the win to the opponent.
- **Recent-moves log** — a persistent panel below the board keeps the last 10 moves/skips visible (including the AI's search score and depth reached for each of its moves), so fast AI-vs-AI play or turn-skips stay legible instead of flashing past.
- **Game-over screen** shows the final board position alongside the result, and waits for a keypress before exiting.
- **Game log export** — at the end of every game, two files are saved to the `logs/` folder:
  - `.log` — raw move sequence on a single line
  - `.txt` — human-readable log with move history, scores, result, and game settings

---

## Puzzle Mode

Puzzle Mode lets you load a custom board position and watch the AI play both sides from that state.

**Setup** (choose via the on-screen menus/text-entry screens):
1. Choose a board input format:
   - **FEN-like string** — rows separated by `/`, using `B`/`b` (Black), `W`/`w` (White), and digits `1–8` for empty cells. Example: `8/8/8/3WB3/3BW3/8/8/8`
   - **64-character string** — left-to-right, top-to-bottom using `B`/`b`, `W`/`w`, and `.` for empty. Example: `...........................WB......BW...........................`
2. Specify whose turn it is (`B` for Black, `W` for White).
3. Set a time limit for the AI (shared by both sides).

The AI then plays out the game from the given position.

---

## Notes on 'AI'
- **Search**: Iterative deepening (IDDFS) with a time limit.
- **Algorithm**: NegaScout (Principal Variation Search).
- **Move ordering**: Moves are tried in priority order (corners → edges → inner cells → X-squares) to improve pruning efficiency.
- **Evaluation components**:
  - *Material*: positional weights per square
  - *Mobility*: relative number of legal moves available
  - *Stability*: discs that can never be flipped (corners, filled edges, enclosed interior)
  - *Frontier*: discs adjacent to empty squares (fewer is better)
- **Dynamic danger correction**: static penalties for X/C-squares are lifted when the adjacent corner is already owned, since those squares are no longer a liability.
