#include "AIPlayer.hpp"
#include "Board.hpp" // For getValidMoves
// AIStrategy.hpp will be included when AIStrategy is implemented
// For now, we don't have the definition of AIStrategy or its methods.

AIPlayer::AIPlayer(char symbol, AIStrategy* aiStrategy, int timeLimit)
    : Player(symbol), strategy(aiStrategy), timeLimitSeconds(timeLimit) {}

std::pair<int, int> AIPlayer::getMove(const Board& board) {
    if (strategy) {
        return strategy->findBestMove(board, playerSymbol, timeLimitSeconds);
    }
    // Fallback or error if strategy is null, though constructor should prevent this.
    // For now, return a "no move" indicator.
    std::cerr << "Error: AIPlayer has no strategy set!" << std::endl; // Should go to a logger
    return {-1, -1};
}

void AIPlayer::setStrategy(AIStrategy* aiStrategy) {
    strategy = aiStrategy;
}
