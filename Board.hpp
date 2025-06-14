#ifndef BOARD_HPP
#define BOARD_HPP

#include "GameConstants.hpp"
#include <vector>
#include <utility> // For std::pair

/**
 * @brief Manages the Othello game board and its state.
 * This class provides functionalities to initialize the board,
 * make moves, check for valid moves, retrieve board information,
 * and determine game status aspects like fullness.
 */
class Board {
private:
    /// @brief The size of one dimension of the square board.
    int boardSize;
    /// @brief 2D vector representing the game grid. Stores player symbols or EMPTY.
    std::vector<std::vector<char>> grid;

public:
    /**
     * @brief Constructs a Board object.
     * @param size The size of the board (number of rows and columns). Defaults to GameConstants::BOARD_SIZE.
     */
    Board(int size = GameConstants::BOARD_SIZE);

    /**
     * @brief Initializes the board to the standard Othello starting position.
     * Clears the board and places the initial four discs in the center.
     */
    void initialize();

    /**
     * @brief Gets the symbol at a specific cell on the board.
     * @param row The row index of the cell.
     * @param col The column index of the cell.
     * @return The character symbol at the cell (GameConstants::PLAYER1, GameConstants::PLAYER2, or GameConstants::EMPTY).
     *         Returns GameConstants::EMPTY if coordinates are out of bounds, though ideally check with isWithinBounds first.
     */
    char getCell(int row, int col) const;

    /**
     * @brief Sets the symbol at a specific cell on the board.
     * Used for placing discs or for internal operations like highlighting valid moves.
     * @param row The row index of the cell.
     * @param col The column index of the cell.
     * @param playerSymbol The player's symbol to place in the cell.
     */
    void setCell(int row, int col, char playerSymbol);

    /**
     * @brief Checks if the given coordinates are within the board boundaries.
     * @param row The row index.
     * @param col The column index.
     * @return True if (row, col) is a valid position on the board, false otherwise.
     */
    bool isWithinBounds(int row, int col) const;

    /**
     * @brief Determines if a move at the given coordinates is valid for the specified player.
     * A move is valid if the cell is empty and placing a disc there outflanks at least one opponent disc.
     * @param row The row index of the proposed move.
     * @param col The column index of the proposed move.
     * @param playerSymbol The symbol of the player making the move.
     * @return True if the move is valid, false otherwise.
     */
    bool isValidMove(int row, int col, char playerSymbol) const;

    /**
     * @brief Applies a player's move to the board.
     * This involves placing the player's disc at (row, col) and flipping all outflanked opponent discs.
     * Assumes the move is valid; behavior for invalid moves is undefined (should be checked with isValidMove first).
     * @param row The row index of the move.
     * @param col The column index of the move.
     * @param playerSymbol The symbol of the player making the move.
     */
    void applyMove(int row, int col, char playerSymbol);

    /**
     * @brief Gets a list of all valid moves available for a given player.
     * @param playerSymbol The symbol of the player whose valid moves are to be found.
     * @return A vector of std::pair<int, int> representing the (row, col) coordinates of all valid moves.
     */
    std::vector<std::pair<int, int>> getValidMoves(char playerSymbol) const;

    /**
     * @brief Checks if the board is full (i.e., no empty cells remain).
     * @return True if the board is full, false otherwise.
     */
    bool isFull() const;

    /**
     * @brief Counts the number of discs on the board for a specific player.
     * @param playerSymbol The symbol of the player whose discs are to be counted.
     * @return The total number of discs belonging to the specified player.
     */
    int countDiscs(char playerSymbol) const;

    /**
     * @brief Prints the current state of the board to the console.
     * Displays the grid with player symbols and empty cells.
     */
    void printBoard() const;

    /**
     * @brief Prints the board to the console, highlighting valid moves for the current player.
     * Valid moves are typically marked with a special character (e.g., GameConstants::VALID).
     * @param playerSymbol The symbol of the player whose valid moves should be highlighted.
     */
    void printHighlightedBoard(char playerSymbol) const;
};

#endif // BOARD_HPP
