#ifndef AIPLAYER_HPP
#define AIPLAYER_HPP

#include "Player.hpp"
#include "GameConstants.hpp" // For DEFAULT_TIME_LIMIT
#include "AIStrategy.hpp"    // Include the full definition of AIStrategy

/**
 * @brief Represents an AI player in the Othello game.
 * This class derives from the abstract Player class. It uses a strategy pattern,
 * delegating the move decision logic to an AIStrategy object (e.g., NegamaxStrategy).
 */
class AIPlayer : public Player {
private:
    /// @brief Pointer to the AI strategy object that determines the best move.
    AIStrategy* strategy;
    /// @brief Time limit in seconds allocated to the AI for making a move.
    int timeLimitSeconds;

public:
    /**
     * @brief Constructs an AIPlayer object.
     * @param symbol The character symbol for this AI player.
     * @param aiStrategy A pointer to an AIStrategy object that will be used to find moves.
     *                   The AIPlayer does not take ownership of this pointer; it must be managed externally.
     * @param timeLimit The time limit in seconds for the AI to make a move. Defaults to GameConstants::DEFAULT_TIME_LIMIT.
     */
    AIPlayer(char symbol, AIStrategy* aiStrategy, int timeLimit = GameConstants::DEFAULT_TIME_LIMIT);

    /**
     * @brief Gets the AI player's next move by consulting its assigned AI strategy.
     * @param board The current state of the game board.
     * @return A std::pair<int, int> representing the (row, col) of the move chosen by the AI strategy.
     *         Returns {-1, -1} if no strategy is set or if the strategy indicates no possible move.
     */
    std::pair<int, int> getMove(const Board& board) override;

    /**
     * @brief Sets or changes the AI strategy used by this player.
     * @param aiStrategy A pointer to the new AIStrategy object.
     *                   The AIPlayer does not take ownership of this pointer.
     */
    void setStrategy(AIStrategy* aiStrategy);
};

#endif // AIPLAYER_HPP
