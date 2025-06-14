#include "Game.hpp"
#include "GameConstants.hpp"
#include "AIPlayer.hpp" // Required for dynamic_cast<AIPlayer*>
#include <iostream>
#include <vector>
#include <string> // Required for std::string in HumanPlayer getMove adaptation
#include <limits> // Required for std::numeric_limits

Game::Game(Player* p1, Player* p2, bool pvp, bool pvai, char humanSymbol)
    : player1(p1), player2(p2), currentPlayer(p1), isPvP(pvp), isPvAI(pvai), humanPlayerSymbolInPvAI(humanSymbol) {
    board.initialize();
}

void Game::start() {
    board.printBoard();
    while (!isGameOver()) {
        playTurn();
        if (!isGameOver()) { // Check again in case playTurn ended the game
           switchPlayer();
        }
    }
    announceWinner();
}

bool Game::isGameOver() const {
    // Game is over if the board is full
    if (board.isFull()) {
        return true;
    }

    // Game is over if neither player has any valid moves
    bool p1_has_moves = !board.getValidMoves(player1->getSymbol()).empty();
    bool p2_has_moves = !board.getValidMoves(player2->getSymbol()).empty();

    return !p1_has_moves && !p2_has_moves;
}

void Game::switchPlayer() {
    currentPlayer = (currentPlayer == player1) ? player2 : player1;
}

void Game::printScores() const {
    int p1Score = board.countDiscs(player1->getSymbol());
    int p2Score = board.countDiscs(player2->getSymbol());
    std::cout << "Scores: "
              << player1->getSymbol() << ": " << p1Score << "  "
              << player2->getSymbol() << ": " << p2Score << std::endl;
}

void Game::announceWinner() const {
    printScores();
    int p1Score = board.countDiscs(player1->getSymbol());
    int p2Score = board.countDiscs(player2->getSymbol());

    if (p1Score > p2Score) {
        std::cout << "Player " << player1->getSymbol() << " wins!" << std::endl;
    } else if (p2Score > p1Score) {
        std::cout << "Player " << player2->getSymbol() << " wins!" << std::endl;
    } else {
        std::cout << "It's a tie!" << std::endl;
    }
}

void Game::playTurn() {
    std::cout << "\nPlayer " << currentPlayer->getSymbol() << "'s turn." << std::endl;

    std::vector<std::pair<int, int>> validMoves = board.getValidMoves(currentPlayer->getSymbol());

    if (validMoves.empty()) {
        std::cout << "No valid moves available. Skipping turn." << std::endl;
        return;
    }

    // In PvAI, if it's AI's turn and human is Player 1, AI is Player 2 ('O')
    // or if human is Player 2, AI is Player 1 ('X')
    // This logic might need refinement based on how players are set up.
    // For now, print highlighted board for human player.
    bool isHumanTurn = false;
    if (isPvP) {
        isHumanTurn = true; // Both are human
    } else if (isPvAI) {
        if (currentPlayer->getSymbol() == humanPlayerSymbolInPvAI) {
            isHumanTurn = true;
        }
    }
    // If not PvP and not PvAI (i.e. AIvAI), or if it's AI's turn in PvAI
    // then isHumanTurn remains false.

    if (isHumanTurn) { // Or more broadly, if the current player is human
         board.printHighlightedBoard(currentPlayer->getSymbol());
    }


    std::pair<int, int> move = currentPlayer->getMove(board);

    // The getMove() method is expected to return a valid move if validMoves was not empty.
    // HumanPlayer::getMove() includes validation. AIPlayer::getMove() should also ensure validity.
    board.applyMove(move.first, move.second, currentPlayer->getSymbol());
    std::cout << "Move applied: " << char('A' + move.second) << (move.first + 1) << std::endl;

    board.printBoard();
    printScores();
}
