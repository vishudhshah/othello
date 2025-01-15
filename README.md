# Othello

Othello game coded in C++. Can play against an 'AI'.

- If you're here, I assume you already know how to play... If not, [here's a link](https://en.wikipedia.org/wiki/Reversi).
- To play the game, run the `main.exe` executable file and follow the prompts.

## Game modes
1. Player vs Player
2. Player vs AI
3. AI vs AI

## Options
- Search depth for AI can be set to any natural number, **default = 5**.
- In game mode 2 (PvE) the player can choose their disk color and consequently, who starts first.

## Notes on 'AI'
- Negamax algorithm used with alpha-beta pruning for efficiency.
- Evaluation function currently considers:
  - Each side's score, weighted by a dynamic map.
  - Tried integrating piece mobility into evaluation function but failed...