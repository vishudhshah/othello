#include "Board.hpp"
#include "GameConstants.hpp"
#include <iostream>
#include <vector>

Board::Board(int size) : boardSize(size), grid(size, std::vector<char>(size, GameConstants::EMPTY)) {}

void Board::initialize() {
    grid.assign(boardSize, std::vector<char>(boardSize, GameConstants::EMPTY));
    int center = boardSize / 2;
    grid[center - 1][center - 1] = GameConstants::PLAYER1;
    grid[center][center] = GameConstants::PLAYER1;
    grid[center - 1][center] = GameConstants::PLAYER2;
    grid[center][center - 1] = GameConstants::PLAYER2;
}

char Board::getCell(int row, int col) const {
    if (isWithinBounds(row, col)) {
        return grid[row][col];
    }
    return GameConstants::EMPTY; // Or throw an exception
}

void Board::setCell(int row, int col, char playerSymbol) {
    if (isWithinBounds(row, col)) {
        grid[row][col] = playerSymbol;
    }
}

bool Board::isWithinBounds(int row, int col) const {
    return row >= 0 && row < boardSize && col >= 0 && col < boardSize;
}

bool Board::isValidMove(int row, int col, char playerSymbol) const {
    if (!isWithinBounds(row, col) || grid[row][col] != GameConstants::EMPTY) {
        return false;
    }

    char opponentSymbol = (playerSymbol == GameConstants::PLAYER1) ? GameConstants::PLAYER2 : GameConstants::PLAYER1;
    bool moveIsValid = false;

    // Check all 8 directions
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0) continue; // Skip the current cell

            int r = row + dr;
            int c = col + dc;
            int count = 0; // Number of opponent discs to flip

            while (isWithinBounds(r, c) && grid[r][c] == opponentSymbol) {
                r += dr;
                c += dc;
                count++;
            }

            if (isWithinBounds(r, c) && grid[r][c] == playerSymbol && count > 0) {
                moveIsValid = true;
                break;
            }
        }
        if (moveIsValid) break;
    }
    return moveIsValid;
}

void Board::applyMove(int row, int col, char playerSymbol) {
    if (!isValidMove(row, col, playerSymbol)) {
        return; // Or throw an exception
    }

    grid[row][col] = playerSymbol;
    char opponentSymbol = (playerSymbol == GameConstants::PLAYER1) ? GameConstants::PLAYER2 : GameConstants::PLAYER1;

    // Flip opponent discs in all 8 directions
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0) continue;

            int r = row + dr;
            int c = col + dc;
            std::vector<std::pair<int, int>> toFlip;

            while (isWithinBounds(r, c) && grid[r][c] == opponentSymbol) {
                toFlip.push_back({r, c});
                r += dr;
                c += dc;
            }

            if (isWithinBounds(r, c) && grid[r][c] == playerSymbol) {
                for (const auto& p : toFlip) {
                    grid[p.first][p.second] = playerSymbol;
                }
            }
        }
    }
}

std::vector<std::pair<int, int>> Board::getValidMoves(char playerSymbol) const {
    std::vector<std::pair<int, int>> validMoves;
    for (int r = 0; r < boardSize; ++r) {
        for (int c = 0; c < boardSize; ++c) {
            if (isValidMove(r, c, playerSymbol)) {
                validMoves.push_back({r, c});
            }
        }
    }
    return validMoves;
}

bool Board::isFull() const {
    for (int r = 0; r < boardSize; ++r) {
        for (int c = 0; c < boardSize; ++c) {
            if (grid[r][c] == GameConstants::EMPTY) {
                return false;
            }
        }
    }
    return true;
}

int Board::countDiscs(char playerSymbol) const {
    int count = 0;
    for (int r = 0; r < boardSize; ++r) {
        for (int c = 0; c < boardSize; ++c) {
            if (grid[r][c] == playerSymbol) {
                count++;
            }
        }
    }
    return count;
}

void Board::printBoard() const {
    std::cout << "  ";
    for (int c = 0; c < boardSize; ++c) {
        std::cout << c << " ";
    }
    std::cout << std::endl;

    for (int r = 0; r < boardSize; ++r) {
        std::cout << r << " ";
        for (int c = 0; c < boardSize; ++c) {
            std::cout << grid[r][c] << " ";
        }
        std::cout << std::endl;
    }
}

void Board::printHighlightedBoard(char playerSymbol) const {
    std::vector<std::pair<int, int>> validMoves = getValidMoves(playerSymbol);
    Board tempBoard = *this; // Create a temporary copy to mark valid moves

    for (const auto& move : validMoves) {
        tempBoard.setCell(move.first, move.second, GameConstants::VALID);
    }

    std::cout << "  ";
    for (int c = 0; c < boardSize; ++c) {
        std::cout << c << " ";
    }
    std::cout << std::endl;

    for (int r = 0; r < boardSize; ++r) {
        std::cout << r << " ";
        for (int c = 0; c < boardSize; ++c) {
            std::cout << tempBoard.getCell(r, c) << " ";
        }
        std::cout << std::endl;
    }
}
