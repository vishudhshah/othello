**Features**
- Save best predicted scores and display line graph at end of game.
- Puzzle mode: Input a board state (as FEN or similar) and see if the AI can solve it.
- Display who is winning and by how much (using current and future eval score).
- GUI.

**Improvements**
- Look into transposition tables.
- Use OOP - refactor the code to use classes for the board, pieces, and AI.
- Implement stability into eval function: A disk is stable if it cannot be flipped (e.g. corner-connected lines).
- Cache valid moves once per board state to avoid recomputation.

- Alternative algorithms: NegaScout, MTD(f).
- Implement reinforcement learning to improve AI – maybe port to python.