# Othello

---

Othello/Reversi game coded in C++. Can play against an 'AI'.

- Since you're here, I assume you already know how to play... If not, [here's a link](https://en.wikipedia.org/wiki/Reversi#Rules).
- To play the game,
  1. Run the `main` binary file and follow the prompts, or
  2. Download the source code and run `make` to compile, then run the `main` binary file.

---

## Game modes
1. Player vs Player
2. Player vs AI
3. AI vs AI

---

## Options
- Search depth for AI can be set to any natural number, **default = 5**.
- In game mode 2 (PvE) the player can choose their disk color and consequently, who starts first.

---

## Notes on 'AI'
- Negamax algorithm used with alpha-beta pruning for efficiency.
- Evaluation function currently considers:
  - Each side's score, weighted by a dynamic map.
  - Piece mobility.