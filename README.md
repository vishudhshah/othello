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
- **Game log export** — at the end of every game, three files are saved to the `logs/` folder:
  - `.log` — raw move sequence on a single line
  - `.txt` — human-readable log with move history, scores, result, and game settings
  - `.csv` — one row per AI move (`move_number,score,depth,date,game_mode,player_color,time_limit_b,time_limit_w`), for graphing or long-term analysis
- **Optional score/depth chart** — set `OTHELLO_GRAPH=1` to also generate a `.png` chart of AI score and search depth per move (via `plot_game.py`, requires `python3` + `matplotlib`). Off by default; the game runs with zero extra dependencies otherwise.

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
- **Board representation**: bitboards — the board is two 64-bit integers (one bit per square, one integer per color) instead of a grid. This makes move generation, flipping, and scoring fast bitwise operations instead of cell-by-cell loops.
- **Opening book**: the first several moves of a game are often looked up instantly instead of searched — see [Opening book](#opening-book) below.
- **Search**: Iterative deepening (IDDFS) with a time limit.
- **Algorithm**: NegaScout (Principal Variation Search) with alpha-beta pruning, sped up by a **transposition table** — a cache of positions already searched, keyed by a **Zobrist hash** (a fingerprint of the board that updates incrementally as moves are made, rather than being recomputed from scratch). This lets the AI skip re-searching positions it's already evaluated (which happens often — different move orders can reach the same board) and search noticeably deeper in the same time limit.
- **Move ordering**: Moves are tried in priority order (corners → edges → inner cells → X-squares) to improve pruning efficiency, with a further boost from replaying whichever move the transposition table says worked best last time.
- **Evaluation components**:
  - *Material*: positional weights per square
  - *Mobility*: relative number of legal moves available
  - *Stability*: discs that can never be flipped (corners, filled edges, enclosed interior)
  - *Frontier*: discs adjacent to empty squares (fewer is better)
- **Dynamic danger correction**: static penalties for X/C-squares are lifted when the adjacent corner is already owned, since those squares are no longer a liability.

---

## Opening book

The AI can consult a small pre-computed book of strong opening moves (`book.dat`) instead of searching from scratch. It's a lookup table from board position → best known move(s), built ahead of time by running the AI's own search over the opening tree.

**Why it helps**: the very first few moves of Othello have been studied and mostly solved, and searching them live every game is wasted effort — a book move is free (returned instantly, no search) and just as strong.

**A few things make it smarter than a plain move list**:
- It's keyed by the *position on the board*, not the sequence of moves that reached it — so different move orders that land on the same board share one book entry.
- It's keyed by *canonical* position — the board's 8 rotations/reflections (spin it 90°, flip it, etc.) are recognized as the same entry, so the book doesn't need a separate copy for every symmetric variation of a line.
- Where multiple opening moves are genuinely equally good, the book stores all of them and picks one at random each time — so the AI doesn't play the exact same opening every single game.

You'll see this in the recent-moves log / CSV export as a move with **depth `-1`** — that's the book's way of saying "no search happened, this came straight from the book."

The book is **only ever used from the standard starting position onward** — it's silently skipped in Puzzle Mode (a custom position essentially never coincidentally matches a book entry), and it hands off to normal search automatically once you're past however many moves the book covers.

### Generating / updating the book

The book is built by running the game itself in a special headless mode — no ncurses, just prints progress and writes a file:

```bash
./main --gen-book --ply 10 --search-depth 10 --epsilon 100 --out book.dat
```

- `--ply <n>` — how many moves deep into the game to build the book (10 = book covers the first 10 moves).
- `--search-depth <n>` — how deep the AI searches to *evaluate* each candidate move while building the book. Higher = stronger/slower book generation.
- `--epsilon <n>` — how close a move's score has to be to the best move to *also* get included (and explored further) as an alternative. `0` = only the single best move is kept (a narrow, deterministic book); higher = more alternatives stored, more variety during play, but a much bigger tree to search — this can blow up generation time fast, so raise it gradually rather than jumping straight to a large value.
- `--out <path>` — where to save the book (defaults to `book.dat`, which is what the game loads automatically on startup).

Regenerating is always safe to re-run — each run starts from a clean slate and fully overwrites the output file, it never adds to what's already there.

To inspect what's actually in a book file (position, move, score for every stored entry):

```bash
./main --book-dump book.dat
```

If `book.dat` is missing, the game just runs without one — search happens from the very first move instead, exactly like before the book existed. No error, no setup required to just play.

---

## Running the game without the TUI (developer/testing mode)

Passing certain flags on the command line skips the ncurses screen entirely and runs one specific thing headlessly, printing plain text to the terminal. Useful for testing, benchmarking, or scripting — not needed for normal play.

```bash
./main --selfplay --time 2                     # play a full AI-vs-AI game, print every move
./main --search --time 5                       # get the AI's move for the current position
./main --search --fen "8/8/8/3WB3/3BW3/8/8/8" --player B   # ...from a specific position
./main --perft 6                                # count reachable positions N moves out (move-generator correctness check)
./main --gen-book --ply 10 --search-depth 10 --epsilon 100 # build/update the opening book (see above)
./main --book-dump book.dat                     # print every entry in a book file
```

Common options across these: `--fen <string>` / `--64 <string>` load a starting position (same formats as Puzzle Mode above; defaults to the standard starting position if neither is given), `--player B|W` sets whose turn it is, `--time <seconds>` sets the AI's thinking time. A couple of flags exist purely for verifying the engine is behaving correctly and aren't needed day-to-day: `--verify-zobrist` (with `--perft`) and `--no-tt` / `--search --fixed-depth <n>` (with `--search`).

Running `./main` with none of these flags starts the normal interactive game exactly as before — nothing about regular play has changed.
