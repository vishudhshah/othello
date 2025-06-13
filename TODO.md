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

**Bugs**

- Didn't play the winning move when it could:
```
Player W's turn.
Best score for W: 85
AI plays: C7

  A B C D E F G H
1 . . . . . . . .
2 . . . _ . . . .
3 . _ W W W _ . .
4 . . W W W . . .
5 . _ W B W _ . .
6 . . W W . . . .
7 . _ W _ . . . .
8 . . . . . . . .
B: 1, W: 11

Move 9
Player B's turn.
Enter your move (eg. A1 or a1): f3

  A B C D E F G H
1 . . . . . . . .
2 . . . . . . _ .
3 . . W W W B _ .
4 . . W W B _ . .
5 . . W B W _ . .
6 . . W W _ . . .
7 . . W . . . . .
8 . . . . . . . .
B: 3, W: 10

Move 10
Player W's turn.
Best score for W: 100
AI plays: E6

  A B C D E F G H
1 . . . . . . . .
2 . . _ . _ . . .
3 . _ W W W B . .
4 . _ W W B . . .
5 . . W W W . . .
6 . . W W W . . .
7 . _ W . _ . . .
8 . . . . . . . .
B: 2, W: 12
```