#include <iostream>
#include <limits>
#include <memory> // For std::unique_ptr, though not strictly necessary if using raw pointers and careful deletion
#include <string> // For std::string in helper functions
#include <cctype> // For std::toupper

#include "Game.hpp"
#include "Player.hpp"
#include "HumanPlayer.hpp"
#include "AIPlayer.hpp"
#include "NegamaxStrategy.hpp"
#include "GameConstants.hpp"

// It's generally better to use specific using declarations if needed, or qualify with std::
// For simplicity in main.cpp, we can use these:
using namespace std;
using namespace GameConstants;

// Helper function to get game mode from user
int getGameMode() {
    int mode;
    cout << "Enter the mode of the game:\n"
         << "1. Player vs Player\n"
         << "2. Player vs AI\n"
         << "3. AI vs AI\n"
         << "Your choice: ";
    while (!(cin >> mode) || mode < 1 || mode > 3) {
        cout << "Invalid input. Please enter a number between 1 and 3: ";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // consume newline
    cout << endl;
    return mode;
}

// Helper function to get time limit from user
int getTimeLimit() {
    int time_limit;
    cout << "Enter the time limit for AI moves (in seconds, e.g., 5): ";
    while (!(cin >> time_limit) || time_limit <= 0) {
        cout << "Invalid input. Please enter a positive integer for time limit: ";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // consume newline
    cout << endl;
    return time_limit;
}

// Helper function to get player color from user
char getPlayerColor() {
    char color;
    string input;
    cout << "Choose your color (X for Player 1, O for Player 2): ";
    while (true) {
        getline(cin, input);
        if (input.length() == 1) {
            color = toupper(input[0]);
            if (color == PLAYER1 || color == PLAYER2) {
                break;
            }
        }
        cout << "Invalid input. Please enter 'X' or 'O': ";
    }
    cout << endl;
    return color;
}

int main() {
    Player* player1 = nullptr;
    Player* player2 = nullptr;
    AIStrategy* aiStrategy = nullptr; // Single strategy object, can be shared if stateless

    int gameMode = getGameMode();
    int timeLimit = DEFAULT_TIME_LIMIT; // Default, can be overridden
    char humanColor = PLAYER1;          // Default, relevant for PvAI

    if (gameMode == 1) { // Player vs Player
        cout << "Player 1 will be " << PLAYER1 << ", Player 2 will be " << PLAYER2 << endl;
        player1 = new HumanPlayer(PLAYER1);
        player2 = new HumanPlayer(PLAYER2);
    } else if (gameMode == 2) { // Player vs AI
        timeLimit = getTimeLimit();
        humanColor = getPlayerColor();
        aiStrategy = new NegamaxStrategy(DEFAULT_DEPTH); // Or allow choosing depth too

        if (humanColor == PLAYER1) {
            cout << "You are Player 1 (X)." << endl;
            player1 = new HumanPlayer(PLAYER1);
            player2 = new AIPlayer(PLAYER2, aiStrategy, timeLimit);
        } else {
            cout << "You are Player 2 (O)." << endl;
            player1 = new AIPlayer(PLAYER1, aiStrategy, timeLimit);
            player2 = new HumanPlayer(PLAYER2);
        }
    } else { // AI vs AI (mode 3)
        timeLimit = getTimeLimit();
        cout << "AI vs AI selected. Player 1 (X) vs Player 2 (O)." << endl;
        // If NegamaxStrategy has internal state that changes during a game,
        // you might need two separate strategy objects.
        // For now, assuming it's reusable/stateless for different players if called sequentially.
        aiStrategy = new NegamaxStrategy(DEFAULT_DEPTH);
        // AIPlayer* aiPlayerForP1 = new AIPlayer(PLAYER1, aiStrategy, timeLimit);
        // AIPlayer* aiPlayerForP2 = new AIPlayer(PLAYER2, aiStrategy, timeLimit);
        // player1 = aiPlayerForP1;
        // player2 = aiPlayerForP2;
        // To be absolutely safe if strategy could have state or if we want different strategies later:
        AIStrategy* aiStrategy2 = new NegamaxStrategy(DEFAULT_DEPTH); // Potentially a different strategy or config
        player1 = new AIPlayer(PLAYER1, aiStrategy, timeLimit);
        player2 = new AIPlayer(PLAYER2, aiStrategy2, timeLimit); // Use second strategy for P2
        // If sharing one strategy instance:
        // player1 = new AIPlayer(PLAYER1, aiStrategy, timeLimit);
        // player2 = new AIPlayer(PLAYER2, aiStrategy, timeLimit);
        // For this example, let's use two distinct strategy objects for AIvAI to be safer and more flexible.
        // The initial plan said "aiStrategy = new NegamaxStrategy(); ... player2 = new AIPlayer(GameConstants::PLAYER2, aiStrategy, timeLimit);"
        // implying sharing. Let's stick to one for now unless issues arise.
        // Deleting aiStrategy2 if it's created:
        // if (aiStrategy2 != aiStrategy) delete aiStrategy2; (This is getting complex, let's simplify for now)
        // Sticking to one strategy object for AIvAI for now as per initial thought,
        // and main will delete it. If AIPlayer took ownership, this would be different.
        // Reverting to single shared strategy for AIvAI for simplicity of cleanup:
        if (player2) delete player2; // Clean up if previous path (like PvAI) created it. Should not happen here.
        if (player1) delete player1;

        aiStrategy = new NegamaxStrategy(DEFAULT_DEPTH); // One strategy for both
        player1 = new AIPlayer(PLAYER1, aiStrategy, timeLimit);
        player2 = new AIPlayer(PLAYER2, aiStrategy, timeLimit);

    }

    // Create and start the game
    // The Game constructor needs to know about PvAI status for highlighted board for human
    bool isPvPGame = (gameMode == 1);
    bool isPvAIGame = (gameMode == 2);

    Game game(player1, player2, isPvPGame, isPvAIGame, humanColor);
    game.start();

    // Cleanup
    cout << "Cleaning up resources..." << endl;
    delete player1; // Deletes HumanPlayer or AIPlayer instance
    player1 = nullptr;
    delete player2; // Deletes HumanPlayer or AIPlayer instance
    player2 = nullptr;

    // aiStrategy is deleted here. If AIPlayer instances were to delete their
    // strategy, this would be a double free if they shared the same strategy object.
    // Current AIPlayer does not delete strategy, so main is responsible.
    delete aiStrategy;
    aiStrategy = nullptr;
    // If we created two separate strategies for AIvAI, the second one would need deletion too.
    // e.g. if (gameMode == 3 && aiStrategy2 != nullptr && aiStrategy2 != aiStrategy) delete aiStrategy2;

    cout << "Game finished. Goodbye!" << endl;

    return 0;
}
