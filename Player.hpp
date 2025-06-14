#ifndef PLAYER_HPP
#define PLAYER_HPP

#include "Board.hpp"
#include "GameConstants.hpp"
#include <utility> // For std::pair

/**
 * @brief Abstract base class representing a player in the Othello game.
 * This class defines the common interface for all player types (e.g., Human, AI).
 * It stores the player's symbol and provides a pure virtual method for getting a move.
 */
class Player {
protected:
    /// @brief The character symbol representing the player (e.g., GameConstants::PLAYER1 or GameConstants::PLAYER2).
    char playerSymbol;

public:
    /**
     * @brief Constructs a Player object.
     * @param symbol The character symbol for this player.
     */
    Player(char symbol);

    /**
     * @brief Virtual destructor.
     * Ensures proper cleanup when deleting derived player objects through a base class pointer.
     */
    virtual ~Player() = default;

    /**
     * @brief Pure virtual method to get the player's next move.
     * Derived classes must implement this method to define how a player decides on a move.
     * @param board The current state of the game board (passed by const reference to prevent modification).
     * @return A std::pair<int, int> representing the (row, col) of the chosen move.
     *         If no valid move is possible, implementations might return a special value like {-1, -1}.
     */
    virtual std::pair<int, int> getMove(const Board& board) = 0;

    /**
     * @brief Gets the symbol of the player.
     * @return The character symbol representing the player.
     */
    char getSymbol() const;
};

#endif // PLAYER_HPP
