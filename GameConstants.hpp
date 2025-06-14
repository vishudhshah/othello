#ifndef GAMECONSTANTS_HPP
#define GAMECONSTANTS_HPP

#include <vector>

/**
 * @brief Defines game-wide constants for Othello.
 * This namespace centralizes all fixed values used throughout the game,
 * such as board dimensions, player symbols, default AI parameters,
 * and evaluation matrices.
 */
namespace GameConstants {
    /// @brief The size of the game board (e.g., 8 for an 8x8 board).
    const int BOARD_SIZE = 8;
    /// @brief Character representing an empty cell on the board.
    const char EMPTY = '.';
    /// @brief Character used to highlight a valid move on the board for the UI.
    const char VALID = '*';
    /// @brief Character representing Player 1 (typically Black).
    const char PLAYER1 = 'X';
    /// @brief Character representing Player 2 (typically White).
    const char PLAYER2 = 'O';
    /// @brief Default search depth for the AI's game tree search.
    const int DEFAULT_DEPTH = 5;
    /// @brief Default time limit in seconds for the AI to make a move.
    const int DEFAULT_TIME_LIMIT = 5; // seconds

    /**
     * @brief Positional weights for board evaluation, typically used in early to mid-game.
     * These values assign strategic importance to different cells on the board.
     * Corners are highly valued, while cells adjacent to corners can be disadvantageous.
     */
    const std::vector<std::vector<int>> POSITION_WEIGHTS = {
        {120, -20, 20,  5,  5, 20, -20, 120},
        {-20, -40, -5, -5, -5, -5, -40, -20},
        { 20,  -5, 15,  3,  3, 15,  -5,  20},
        {  5,  -5,  3,  3,  3,  3,  -5,   5},
        {  5,  -5,  3,  3,  3,  3,  -5,   5},
        { 20,  -5, 15,  3,  3, 15,  -5,  20},
        {-20, -40, -5, -5, -5, -5, -40, -20},
        {120, -20, 20,  5,  5, 20, -20, 120}
    };

    /**
     * @brief Positional weights for board evaluation, typically used in the endgame.
     * In the endgame, the focus shifts more towards maximizing disc count,
     * but positional advantages can still play a role.
     */
    const std::vector<std::vector<int>> ENDGAME_WEIGHTS = {
        {500, -150, 30, 10, 10, 30, -150, 500},
        {-150, -250,  0,  0,  0,  0, -250, -150},
        {  30,    0,  1,  2,  2,  1,    0,   30},
        {  10,    0,  2, 16, 16,  2,    0,   10},
        {  10,    0,  2, 16, 16,  2,    0,   10},
        {  30,    0,  1,  2,  2,  1,    0,   30},
        {-150, -250,  0,  0,  0,  0, -250, -150},
        { 500, -150, 30, 10, 10, 30, -150,  500}
    };
}

#endif // GAMECONSTANTS_HPP
