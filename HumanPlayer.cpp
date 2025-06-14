#include "HumanPlayer.hpp"
#include "GameConstants.hpp" // For BOARD_SIZE
#include <iostream>
#include <string>
#include <limits>   // Required for std::numeric_limits
#include <cctype>   // Required for std::toupper

HumanPlayer::HumanPlayer(char symbol) : Player(symbol) {}

std::pair<int, int> HumanPlayer::getMove(const Board& board) {
    std::string userInput;
    int row, col;
    int boardSize = board.isFull() ? GameConstants::BOARD_SIZE : GameConstants::BOARD_SIZE; // Placeholder, real board size needed from Board class if dynamic

    // Get user input
    std::cout << "Player " << playerSymbol << ", enter your move (e.g., A1 or a1): ";

    while (true) {
        std::cin >> userInput;

        // Validate the user input format
        if (userInput.length() == 2 &&
            std::isalpha(userInput[0]) &&
            std::isdigit(userInput[1])) {

            col = std::toupper(userInput[0]) - 'A';
            row = userInput[1] - '1';

            if (board.isWithinBounds(row, col)) {
                if (board.isValidMove(row, col, playerSymbol)) {
                    break; // Valid move
                } else {
                    std::cout << "Invalid move. That square is not a valid move. Try again: ";
                }
            } else {
                 std::cout << "Invalid input. Move out of bounds. Try again (e.g., A1): ";
            }
        } else {
            std::cout << "Invalid input format. Try again (e.g., A1): ";
        }

        // Clear error flags and discard invalid input
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    return std::make_pair(row, col);
}
