#ifndef NEGAMAXSTRATEGY_HPP
#define NEGAMAXSTRATEGY_HPP

#include "AIStrategy.hpp"
#include "Board.hpp"
#include "GameConstants.hpp"
#include <vector>
#include <utility> // For std::pair
#include <chrono>  // For time management
#include <algorithm> // For std::sort, std::max, std::min

/**
 * @brief Implements the AIStrategy interface using the Negamax algorithm with Alpha-Beta pruning.
 * This strategy performs a depth-limited game tree search to find the optimal move
 * for the current player, considering positional heuristics and game phase.
 * It employs iterative deepening to manage the allocated time effectively.
 */
class NegamaxStrategy : public AIStrategy {
public:
    /**
     * @brief Constructs a NegamaxStrategy object.
     * @param defaultDepth The default maximum depth for the iterative deepening search.
     *                     Defaults to GameConstants::DEFAULT_DEPTH.
     */
    NegamaxStrategy(int defaultDepth = GameConstants::DEFAULT_DEPTH);

    /**
     * @brief Finds the best move for the given player using the Negamax algorithm.
     * This method uses iterative deepening search up to a specified maximum depth or until the time limit is reached.
     * @param board The current state of the game board.
     * @param playerSymbol The symbol of the player for whom to find the best move.
     * @param timeLimitSeconds The maximum time in seconds allowed for the search.
     * @return A std::pair<int, int> representing the (row, col) of the best move found.
     *         Returns {-1, -1} if no valid moves are available.
     */
    std::pair<int, int> findBestMove(const Board& board, char playerSymbol, int timeLimitSeconds) override;

private:
    /**
     * @brief The core Negamax algorithm with Alpha-Beta pruning.
     * Recursively explores the game tree to a given depth to find the best score for the current player.
     * @param currentBoard The current board state for this node of the search. (Passed by value for modification)
     * @param depth The remaining depth to search.
     * @param alpha The alpha value for alpha-beta pruning (best score found so far for the maximizing player).
     * @param beta The beta value for alpha-beta pruning (best score found so far for the minimizing player).
     * @param playerSymbol The symbol of the player whose turn it is at this node.
     * @param isMaximizingPlayer True if the current player at this node is trying to maximize the score,
     *                           false if trying to minimize. This helps align with the Negamax principle where
     *                           score = -negamax(opponent).
     * @param startTime The time point when the overall `findBestMove` search started. Used for time limit checks.
     * @param timeLimitMs The total time limit in milliseconds for the `findBestMove` operation.
     * @return The evaluated score of the board from the perspective of `playerSymbol` if `isMaximizingPlayer` is true,
     *         otherwise from the perspective of the opponent. Returns special values if timeout occurs.
     */
    int negamax(Board currentBoard, int depth, int alpha, int beta, char playerSymbol, bool isMaximizingPlayer, std::chrono::steady_clock::time_point startTime, int timeLimitMs);

    /**
     * @brief Evaluates the given board state for a specific player.
     * The evaluation considers material score (disc count adjusted by positional weights)
     * and mobility (number of valid moves). The importance of these factors can change based on the game phase.
     * @param board The board state to evaluate.
     * @param playerSymbol The symbol of the player from whose perspective the board is evaluated.
     * @return An integer score representing the desirability of the board state for `playerSymbol`.
     *         Higher scores are better for `playerSymbol`.
     */
    int evaluateBoard(const Board& board, char playerSymbol);

    /**
     * @brief Determines the current phase of the game based on the number of discs on the board.
     * Game phases can influence evaluation heuristics (e.g., positional play vs. maximizing disc count).
     * @param board The current board state.
     * @return An integer representing the game phase (e.g., 1 for early game, 4 for endgame).
     */
    int getGamePhase(const Board& board) const;

    /**
     * @brief Gets a list of valid moves for a player, sorted by a heuristic.
     * Sorting moves can improve the efficiency of alpha-beta pruning by exploring promising moves earlier.
     * @param board The current board state.
     * @param playerSymbol The symbol of the player whose moves are to be sorted.
     * @return A vector of std::pair<int, int> representing sorted (row, col) coordinates of valid moves.
     */
    std::vector<std::pair<int, int>> getSortedMoves(const Board& board, char playerSymbol);

    /// @brief The maximum search depth for the iterative deepening process.
    int maxDepth;
    /// @brief Stores the start time of the current `findBestMove` iteration (for iterative deepening).
    std::chrono::steady_clock::time_point iterationStartTime;
    /// @brief The time limit in milliseconds for the current `findBestMove` operation.
    int iterationTimeLimitMs;

    /**
     * @brief Helper function to get the opponent's symbol.
     * @param playerSymbol The symbol of the current player.
     * @return The symbol of the opponent.
     */
    char getOpponentSymbol(char playerSymbol) const {
        return (playerSymbol == GameConstants::PLAYER1) ? GameConstants::PLAYER2 : GameConstants::PLAYER1;
    }
};

#endif // NEGAMAXSTRATEGY_HPP
