#ifndef GAME_HPP
#define GAME_HPP

#include "Board.hpp"
#include "Player.hpp"
#include "GameConstants.hpp"

/**
 * @brief Manages the overall Othello game flow.
 * This class coordinates players, the board, and game rules.
 * It handles turn management, game state checking (game over),
 * and orchestrates the main game loop.
 */
class Game {
private:
    /// @brief The game board instance.
    Board board;
    /// @brief Pointer to Player 1.
    Player* player1;
    /// @brief Pointer to Player 2.
    Player* player2;
    /// @brief Pointer to the player whose turn it currently is.
    Player* currentPlayer;
    /// @brief Flag indicating if the game is Player vs Player.
    bool isPvP;
    /// @brief Flag indicating if the game is Player vs AI.
    bool isPvAI;
    /// @brief Stores the symbol of the human player in a Player vs AI game. Useful for UI hints.
    char humanPlayerSymbolInPvAI;

public:
    /**
     * @brief Constructs a Game object.
     * @param p1 Pointer to Player 1. The Game object does not take ownership of this pointer.
     * @param p2 Pointer to Player 2. The Game object does not take ownership of this pointer.
     * @param pvp True if the game is Player vs Player, false otherwise.
     * @param pvai True if the game is Player vs AI, false otherwise.
     * @param humanSymbol The symbol of the human player if in PvAI mode (e.g., GameConstants::PLAYER1).
     *                    Defaults to GameConstants::PLAYER1. This helps in providing relevant UI cues.
     */
    Game(Player* p1, Player* p2, bool pvp = false, bool pvai = false, char humanSymbol = GameConstants::PLAYER1);

    /**
     * @brief Starts and manages the main game loop.
     * This loop continues until the game is over, handling turns,
     * board updates, and player switching.
     */
    void start();

    /**
     * @brief Checks if the game is over.
     * The game is over if the board is full or if neither player has any valid moves.
     * @return True if the game is over, false otherwise.
     */
    bool isGameOver() const;

    /**
     * @brief Switches the current player to the other player.
     */
    void switchPlayer();

    /**
     * @brief Prints the current scores (number of discs) for both players to the console.
     */
    void printScores() const;

    /**
     * @brief Announces the winner of the game or declares a tie.
     * This is typically called after the game loop in start() finishes.
     */
    void announceWinner() const;

    /**
     * @brief Manages a single turn for the current player.
     * This includes getting the player's move, applying it to the board,
     * printing the board, and handling cases like skipped turns.
     */
    void playTurn();
};

#endif // GAME_HPP
