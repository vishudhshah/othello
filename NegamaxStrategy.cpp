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


int NegamaxStrategy::negamax(Board currentBoard, int depth, int alpha, int beta, char activePlayerSymbol, std::chrono::steady_clock::time_point globalStartTime, int globalTimeLimitMs) {
    if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - globalStartTime).count() > globalTimeLimitMs) {
        return alpha; // Timeout: return a safe "best guess so far" or 0 if preferred. Alpha is a cautious choice.
    }

    // Check for game over condition for the currentBoard state
    char opponentSymbol = getOpponentSymbol(activePlayerSymbol);
    bool activePlayerHasMoves = !currentBoard.getValidMoves(activePlayerSymbol).empty();
    bool opponentHasMoves = !currentBoard.getValidMoves(opponentSymbol).empty();
    bool gameIsOver = currentBoard.isFull() || (!activePlayerHasMoves && !opponentHasMoves);

    if (depth == 0 || gameIsOver) {
        return evaluateBoard(currentBoard, activePlayerSymbol);
    }

    std::vector<std::pair<int, int>> moves = getSortedMoves(currentBoard, activePlayerSymbol);

    if (moves.empty()) { // No valid moves for activePlayerSymbol, pass turn
        // Opponent also has no moves (already checked by gameIsOver), this is a terminal state for score.
        // Actually, gameIsOver would catch this. If only current player has no moves, then it's a pass.
        return -negamax(currentBoard, depth - 1, -beta, -alpha, opponentSymbol, globalStartTime, globalTimeLimitMs);
    }

    int maxEval = std::numeric_limits<int>::min();
    for (const auto& move : moves) {
        Board nextBoard = currentBoard;
        nextBoard.applyMove(move.first, move.second, activePlayerSymbol);
        int eval = -negamax(nextBoard, depth - 1, -beta, -alpha, opponentSymbol, globalStartTime, globalTimeLimitMs);

        maxEval = std::max(maxEval, eval);
        alpha = std::max(alpha, eval);
        if (alpha >= beta) {
            break; // Pruning
        }
    }
    return maxEval;
}


std::pair<int, int> NegamaxStrategy::findBestMove(const Board& board, char playerSymbol, int timeLimitSeconds) {
    iterationStartTime = std::chrono::steady_clock::now(); // This is the global start time for the entire findBestMove call
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
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - iterationStartTime).count() >= iterationTimeLimitMs) {
            break;
        }

        std::pair<int, int> currentBestMoveThisIteration = validMoves[0]; // Default to first move
        int currentBestScoreThisIteration = std::numeric_limits<int>::min();
        bool completedThisDepth = true;

        for (const auto& move : validMoves) {
             if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - iterationStartTime).count() >= iterationTimeLimitMs) {
                completedThisDepth = false;
                break;
            }

            Board nextBoard = board;
            nextBoard.applyMove(move.first, move.second, playerSymbol);

            // The score returned by negamax is from the perspective of the player whose turn it is *in that call*.
            // Since it's opponent's turn on nextBoard, we negate the result.
            int score = -negamax(nextBoard, currentDepth - 1, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), getOpponentSymbol(playerSymbol), iterationStartTime, iterationTimeLimitMs);

            if (score > currentBestScoreThisIteration) {
                currentBestScoreThisIteration = score;
                currentBestMoveThisIteration = move;
            }
        }

        if (completedThisDepth) {
            bestMoveSoFar = currentBestMoveThisIteration;
            bestScoreSoFar = currentBestScoreThisIteration;
        } else {
            // Time ran out for this depth, use results from previous completed depth (already stored in bestMoveSoFar)
            break;
        }
        // If maxDepth is very high, and time limit is short, this prevents overly long searches.
        if (currentDepth >= GameConstants::BOARD_SIZE * GameConstants::BOARD_SIZE - board.countDiscs(GameConstants::EMPTY) ) break; // Max practical depth based on empty cells
    }

    return bestMoveSoFar;
}
