#ifndef AISTRATEGY_HPP
#define AISTRATEGY_HPP

#include "Board.hpp" // For Board type
#include <utility>  // For std::pair

/**
 * @brief Abstract base class for AI move-finding strategies in Othello.
 * This class defines the interface for different AI algorithms (e.g., Negamax, Monte Carlo)
 * to determine the best move given a board state, player symbol, and time limit.
 */
class AIStrategy {
public:
    /**
     * @brief Virtual destructor.
     * Ensures proper cleanup when deleting derived strategy objects via a base class pointer.
     */
    virtual ~AIStrategy() = default;

    /**
     * @brief Pure virtual method to find the best move for a given player.
     * Derived classes must implement this method to define their specific AI logic.
     * @param board The current state of the game board (passed by const reference).
     * @param playerSymbol The symbol of the player for whom to find the best move.
     * @param timeLimitSeconds The maximum time in seconds allowed for the strategy to find a move.
     * @return A std::pair<int, int> representing the (row, col) of the best move found.
     *         If no move can be found or an error occurs, it might return a special value like {-1, -1}.
     */
    virtual std::pair<int, int> findBestMove(const Board& board, char playerSymbol, int timeLimitSeconds) = 0;
};

#endif // AISTRATEGY_HPP
