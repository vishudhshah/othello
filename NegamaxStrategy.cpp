#include "NegamaxStrategy.hpp"
#include "GameConstants.hpp"
#include <iostream> // For debugging, remove later
#include <algorithm> // For std::sort, std::max, std::min
#include <limits>    // For std::numeric_limits

NegamaxStrategy::NegamaxStrategy(int defaultDepth) : maxDepth(defaultDepth) {}

// Helper to get opponent symbol - can be part of the class or local if not used elsewhere
// char getOpponentSymbol(char playerSymbol) {
//     return (playerSymbol == GameConstants::PLAYER1) ? GameConstants::PLAYER2 : GameConstants::PLAYER1;
// }

int NegamaxStrategy::getGamePhase(const Board& board) const {
    int totalDiscs = GameConstants::BOARD_SIZE * GameConstants::BOARD_SIZE - board.countDiscs(GameConstants::EMPTY);
    if (totalDiscs <= 20) return 1; // Early game
    if (totalDiscs <= 48) return 2; // Mid game
    if (totalDiscs <= 60) return 3; // Late game
    return 4; // Endgame
}

int NegamaxStrategy::evaluateBoard(const Board& board, char playerSymbol) {
    int playerScore = 0;
    int opponentScore = 0;
    char opponentSymbol = getOpponentSymbol(playerSymbol);

    int phase = getGamePhase(board);
    const auto& weights = (phase < 3) ? GameConstants::POSITION_WEIGHTS : GameConstants::ENDGAME_WEIGHTS;

    for (int r = 0; r < GameConstants::BOARD_SIZE; ++r) {
        for (int c = 0; c < GameConstants::BOARD_SIZE; ++c) {
            char cell = board.getCell(r, c);
            if (cell == playerSymbol) {
                playerScore += (phase == 4) ? 1 : weights[r][c];
            } else if (cell == opponentSymbol) {
                opponentScore += (phase == 4) ? 1 : weights[r][c];
            }
        }
    }

    int materialScore = playerScore - opponentScore;

    // Mobility score (number of valid moves for the current player)
    // This was for the *opponent* in the original, let's make it for current playerSymbol
    int mobilityScore = 0;
    if (phase < 4) { // Mobility is less important in the pure disc-counting endgame
        int numValidMoves = board.getValidMoves(playerSymbol).size();
        int numOpponentValidMoves = board.getValidMoves(opponentSymbol).size();
        mobilityScore = 10 * (numValidMoves - numOpponentValidMoves); // Relative mobility
    }

    return materialScore + mobilityScore;
}


std::vector<std::pair<int, int>> NegamaxStrategy::getSortedMoves(const Board& board, char playerSymbol) {
    std::vector<std::pair<int, int>> validMoves = board.getValidMoves(playerSymbol);

    // Simple heuristic: prefer corners, then edges, then other moves.
    // This is a basic sort, can be improved.
    std::sort(validMoves.begin(), validMoves.end(), [&](const auto& a, const auto& b) {
        bool aIsCorner = (a.first == 0 || a.first == GameConstants::BOARD_SIZE - 1) && (a.second == 0 || a.second == GameConstants::BOARD_SIZE - 1);
        bool bIsCorner = (b.first == 0 || b.first == GameConstants::BOARD_SIZE - 1) && (b.second == 0 || b.second == GameConstants::BOARD_SIZE - 1);
        if (aIsCorner != bIsCorner) return aIsCorner;

        bool aIsEdge = (a.first == 0 || a.first == GameConstants::BOARD_SIZE - 1 || a.second == 0 || a.second == GameConstants::BOARD_SIZE - 1);
        bool bIsEdge = (b.first == 0 || b.first == GameConstants::BOARD_SIZE - 1 || b.second == 0 || b.second == GameConstants::BOARD_SIZE - 1);
        if (aIsEdge != bIsEdge) return aIsEdge;

        // Could use evaluation of the move here if needed for more advanced sorting
        return false; // Default if no other criteria
    });
    return validMoves;
}

int NegamaxStrategy::negamax(Board currentBoard, int depth, int alpha, int beta, char playerSymbol, bool isMaximizingPlayer, std::chrono::steady_clock::time_point startTime, int timeLimitMs) {

    if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count() > timeLimitMs) {
        return isMaximizingPlayer ? std::numeric_limits<int>::min() + 1 : std::numeric_limits<int>::max() -1; // Indicate timeout
    }

    char originalPlayerSymbol = playerSymbol; // The player for whom we are maximizing the score at this node initially.
                                          // This is the player whose turn it is for `currentBoard`.

    // Check for game over condition for the currentBoard state
    bool p1_has_moves = !currentBoard.getValidMoves(GameConstants::PLAYER1).empty();
    bool p2_has_moves = !currentBoard.getValidMoves(GameConstants::PLAYER2).empty();
    bool gameIsOver = currentBoard.isFull() || (!p1_has_moves && !p2_has_moves);

    if (depth == 0 || gameIsOver) {
        int eval = evaluateBoard(currentBoard, originalPlayerSymbol);
        return isMaximizingPlayer ? eval : -eval;
    }

    std::vector<std::pair<int, int>> moves = getSortedMoves(currentBoard, playerSymbol);

    if (moves.empty()) {
        // No valid moves for current player, pass turn.
        // Score is evaluated from the perspective of the next player, then negated.
        return -negamax(currentBoard, depth -1 , -beta, -alpha, getOpponentSymbol(playerSymbol), !isMaximizingPlayer, startTime, timeLimitMs);
    }

    int bestValue;
    if (isMaximizingPlayer) {
        bestValue = std::numeric_limits<int>::min();
        for (const auto& move : moves) {
            Board nextBoard = currentBoard;
            nextBoard.applyMove(move.first, move.second, playerSymbol);
            int value = negamax(nextBoard, depth - 1, alpha, beta, getOpponentSymbol(playerSymbol), false, startTime, timeLimitMs);
            bestValue = std::max(bestValue, value);
            alpha = std::max(alpha, value);
            if (beta <= alpha) {
                break; // Beta cut-off
            }
        }
    } else { // Minimizing player (opponent's turn from the perspective of original player)
        bestValue = std::numeric_limits<int>::max();
        for (const auto& move : moves) {
            Board nextBoard = currentBoard;
            nextBoard.applyMove(move.first, move.second, playerSymbol);
            // The recursive call is for the other player, so isMaximizingPlayer becomes true.
            // The score returned by negamax will be from playerSymbol's perspective (original player's opponent).
            // We want to minimize this score.
            int value = negamax(nextBoard, depth - 1, alpha, beta, getOpponentSymbol(playerSymbol), true, startTime, timeLimitMs);
            bestValue = std::min(bestValue, value);
            beta = std::min(beta, value);
            if (beta <= alpha) {
                break; // Alpha cut-off
            }
        }
    }
    return bestValue;
}


std::pair<int, int> NegamaxStrategy::findBestMove(const Board& board, char playerSymbol, int timeLimitSeconds) {
    iterationStartTime = std::chrono::steady_clock::now();
    iterationTimeLimitMs = timeLimitSeconds * 1000;

    std::vector<std::pair<int, int>> validMoves = getSortedMoves(board, playerSymbol);

    if (validMoves.empty()) {
        return {-1, -1}; // No valid moves
    }
    if (validMoves.size() == 1) {
        return validMoves[0]; // Only one move, return it
    }

    std::pair<int, int> bestMoveSoFar = validMoves[0];
    int bestScoreSoFar = std::numeric_limits<int>::min();

    // Iterative Deepening
    for (int currentDepth = 1; currentDepth <= maxDepth; ++currentDepth) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - iterationStartTime).count() > iterationTimeLimitMs) {
            std::cout << "Time limit reached before starting depth " << currentDepth << std::endl;
            break;
        }
        std::cout << "Searching at depth: " << currentDepth << std::endl;

        std::pair<int, int> currentBestMoveThisIteration = validMoves[0];
        int bestScoreThisIteration = std::numeric_limits<int>::min();
        bool completedThisDepth = true;

        for (const auto& move : validMoves) {
             if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - iterationStartTime).count() > iterationTimeLimitMs) {
                std::cout << "Time limit reached during depth " << currentDepth << std::endl;
                completedThisDepth = false;
                break;
            }

            Board nextBoard = board;
            nextBoard.applyMove(move.first, move.second, playerSymbol);

            // Call negamax for the opponent. The score returned is from the opponent's perspective.
            // So, a high score for opponent is bad for us (playerSymbol).
            // We negate it to get the score from playerSymbol's perspective.
            // The 'isMaximizingPlayer' for the first call to negamax should effectively be 'false'
            // because we are evaluating the state *after* playerSymbol has made a move,
            // and it's now opponent's turn.
            int score = -negamax(nextBoard, currentDepth - 1, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), getOpponentSymbol(playerSymbol), true, iterationStartTime, iterationTimeLimitMs);

            if (score > bestScoreThisIteration) {
                bestScoreThisIteration = score;
                currentBestMoveThisIteration = move;
            }
        }

        if (completedThisDepth) {
            bestMoveSoFar = currentBestMoveThisIteration;
            bestScoreSoFar = bestScoreThisIteration;
            std::cout << "Completed depth " << currentDepth << ". Best move: (" << bestMoveSoFar.first << "," << bestMoveSoFar.second << ") with score: " << bestScoreSoFar << std::endl;
        } else {
             std::cout << "Did not complete depth " << currentDepth << ". Using results from depth " << (currentDepth -1) << std::endl;
            break; // Time ran out for this depth, use results from previous completed depth
        }
         // If maxDepth is very high, and time limit is short, this prevents overly long searches.
        if (currentDepth == GameConstants::BOARD_SIZE * GameConstants::BOARD_SIZE) break; // Max practical depth
    }

    std::cout << "Final best move for " << playerSymbol << ": (" << bestMoveSoFar.first << "," << bestMoveSoFar.second << ") with score: " << bestScoreSoFar << std::endl;
    return bestMoveSoFar;
}
