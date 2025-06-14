**Features**
- Puzzle mode: Input a board state (as FEN?) and see if the AI can solve it.
- In AI vs AI mode, allow different search depths.
- Allow going back a move – keep a record of the board state before/after every move.
- Display who is winning and by how much (using current and future eval score).
- GUI.
- Exporting games as PGN.

**Improvements**
- Set depth limit to number of empty squares left so AI doesn't search too deep when the board is almost full.
- Look into transposition tables.
- Use OOP - refactor the code to use classes for the board, pieces, and AI.
- Implement stability into eval function: A disk is stable if it cannot be flipped (e.g. corner-connected lines).
- Cache valid moves once per board state to avoid recomputation.

- Alternative algorithms: NegaScout, MTD(f).
- Implement reinforcement learning to improve AI – maybe port to python.