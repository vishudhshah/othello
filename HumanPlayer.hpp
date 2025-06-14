#ifndef HUMANPLAYER_HPP
#define HUMANPLAYER_HPP

#include "Player.hpp"
#include "Board.hpp" // Included for the declaration of getMove, which takes a Board

/**
 * @brief Represents a human player in the Othello game.
 * This class derives from the abstract Player class and implements the getMove
 * method by prompting the human user for input via the console.
 */
class HumanPlayer : public Player {
public:
    /**
     * @brief Constructs a HumanPlayer object.
     * @param symbol The character symbol for this human player (e.g., GameConstants::PLAYER1 or GameConstants::PLAYER2).
     */
    HumanPlayer(char symbol);

    /**
     * @brief Gets the human player's next move by prompting for input from the console.
     * It will continuously prompt until a valid move format is entered and the move
     * is legal on the provided board.
     * @param board The current state of the game board. Used to validate the move.
     * @return A std::pair<int, int> representing the (row, col) of the chosen valid move.
     */
    std::pair<int, int> getMove(const Board& board) override;
};

#endif // HUMANPLAYER_HPP
