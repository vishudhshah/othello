// IMPORTS

#include <iostream>
#include <vector>
#include <utility>
#include <cmath>

using namespace std;


// GLOBAL VARIABLES

const int BOARD_SIZE = 8;
const char EMPTY = '.';
const char VALID = '_';
const char PLAYER1 = 'B';
const char PLAYER2 = 'W';
const int DEFAULT_DEPTH = 5;

vector<vector<char>> board(BOARD_SIZE, vector<char>(BOARD_SIZE, EMPTY));

// Weighted piece counter for the evaluation function
const int POSITION_WEIGHTS[BOARD_SIZE][BOARD_SIZE] = {
    {100, -10, 20, 20, 20, 20, -10, 100},
    {-10, -20,  1,  1,  1,  1, -20, -10},
    { 20,   1,  5,  5,  5,  5,   1,  20},
    { 20,   1,  5,  5,  5,  5,   1,  20},
    { 20,   1,  5,  5,  5,  5,   1,  20},
    { 20,   1,  5,  5,  5,  5,   1,  20},
    {-10, -20,  1,  1,  1,  1, -20, -10},
    {100, -10, 20, 20, 20, 20, -10, 100}
};

// Weighted piece counter for endgame
const int ENDGAME_WEIGHTS[BOARD_SIZE][BOARD_SIZE] = {
    {100,   5, 10, 10, 10, 10,   5, 100},
    {  5,  -5,  1,  1,  1,  1,  -5,   5},
    { 10,   1,  1,  1,  1,  1,   1,  10},
    { 10,   1,  1,  1,  1,  1,   1,  10},
    { 10,   1,  1,  1,  1,  1,   1,  10},
    { 10,   1,  1,  1,  1,  1,   1,  10},
    {  5,  -5,  1,  1,  1,  1,  -5,   5},
    {100,   5, 10, 10, 10, 10,   5, 100}
};

// Last 10 moves uses a uniform weight matrix (because at this point all you care about is material, not position)


// FUNCTION DECLARATIONS

/**
 * @brief Initialize the starting board configuration
 * 
 * @param board The 2D vector representing the board
 */
void initialize_board() {
    board[3][3] = PLAYER2;
    board[3][4] = PLAYER1;
    board[4][3] = PLAYER1;
    board[4][4] = PLAYER2;
}

/**
 * @brief Print the board
 */
void print_board() {
    char column_labels[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'};

    cout << "  ";
    for (char label : column_labels) {
        cout << label << " ";
    }
    cout << endl;

    for (int i = 0; i < BOARD_SIZE; i++) {
        cout << i + 1 << " ";
        for (int j = 0; j < BOARD_SIZE; j++) {
            cout << board[i][j] << " ";
        }
        cout << endl;
    }
}

/**
 * @brief Get user input for row and column
 * 
 * Takes input in the format "A1" or "a1" and converts it to the corresponding row and column
 * 
 * @return A pair of integers representing the row and column
 */
pair<int, int> get_user_input() {
    string user_input;
    int row, col;

    // Get user input
    cout << "Enter your move (eg. A1 or a1): ";
    cin >> user_input;

    // Validate the user input
    while (user_input.length() != 2 || user_input[0] < 'A' || user_input[0] > 'H' && user_input[0] < 'a' || user_input[0] > 'h' || user_input[1] < '1' || user_input[1] > '8') {
        cout << "Please enter a valid move (eg. A1 or a1): ";
        cin >> user_input;
    }

    // Convert the user input to row and column
    row = user_input[1] - '1';
    col = toupper(user_input[0]) - 'A';

    return make_pair(row, col);

}

/**
 * @brief Make a move on the board and flip the opponent's pieces
 * 
 * @param row The row of the move
 * @param col The column of the move
 * @param player The player making the move
 */
void make_move(int row, int col, char player) {
    board[row][col] = player;

    // Check for opponent pieces in all eight directions
    int directions[8][2] = {{1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}, {0, 1}, {1, 1}};
    for (int i = 0; i < 8; i++) {
        int dx = directions[i][0];
        int dy = directions[i][1];
        int x = col + dx;
        int y = row + dy;

        // Check if there is an opponent piece in the current direction
        if (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE && board[y][x] != player && board[y][x] != EMPTY) {
            // Move in the current direction until reaching the boundary or a player piece or an empty cell
            while (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE && board[y][x] != player && board[y][x] != EMPTY) {
                x += dx;
                y += dy;
            }

            // Flip all opponent pieces in the current direction, going backwards
            if (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE && board[y][x] == player) {
                x -= dx;
                y -= dy;
                while (x != col || y != row) {
                    board[y][x] = player;
                    x -= dx;
                    y -= dy;
                }
            }
        }
    }
}

/**
 * @brief Switch the current player
 * 
 * @param currentPlayer The variable storing the current player
 */
void switch_player(char &currentPlayer) {
    currentPlayer = (currentPlayer == PLAYER1) ? PLAYER2 : PLAYER1;
}

/**
 * @brief Get the opponent's disk color
 * 
 * @param player The current player
 * @return The opposing player
 */
char get_opponent(char player) {
    return (player == PLAYER1) ? PLAYER2 : PLAYER1;
}

/**
 * @brief Calculate the scores of both players
 * 
 * @return A pair of integers representing the scores of both players
 */
pair<int, int> calculate_scores() {
    // Initialize counters for both scores
    int player1_score = 0;
    int player2_score = 0;

    // Iterate through all cells in the board and count the number of pieces for each player
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == PLAYER1) {
                player1_score++;
            } else if (board[i][j] == PLAYER2) {
                player2_score++;
            }
        }
    }

    return make_pair(player1_score, player2_score);
}

/**
 * @brief Print the scores of both players
 */
void print_scores() {
    // Calculate the scores of both players
    pair<int, int> scores = calculate_scores();
    int player1_score = scores.first;
    int player2_score = scores.second;

    // Print the scores of both players
    cout << PLAYER1 << ": " << player1_score << ", " << PLAYER2 << ": " << player2_score << endl;
}

/**
 * @brief Print the winning message
 */
void print_winning_message() {
    // Calculate how much did the player win by
    pair<int, int> scores = calculate_scores();
    int player1_score = scores.first;
    int player2_score = scores.second;
    int winner_score = (player1_score > player2_score) ? player1_score : player2_score;
    int loser_score = (player1_score < player2_score) ? player1_score : player2_score;
    int score_difference = winner_score - loser_score;

    cout << "Game Over.";
    if (score_difference == 0) {
        cout << " It's a tie!" << endl;
    } else {
        cout << " Player " << ((player1_score > player2_score) ? PLAYER1 : PLAYER2) << " won by " << score_difference << " points!" << endl;
    }
}

/**
 * @brief Check if a move is valid
 * 
 * @param row The row of the move
 * @param col The column of the move
 * @param player The player making the move
 * @return A boolean indicating if the move is valid
 */
bool is_valid_move(int row, int col, char player) {
    // Check if the position is within bounds and the cell is empty
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE || board[row][col] != EMPTY) {
        return false;
    }

    // Check for opponent pieces in all eight directions
    // Create Cartesian coordinate system for directions and iterate through each direction
    int directions[8][2] = {{1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}, {0, 1}, {1, 1}};
    for (int i = 0; i < 8; i++) {
        int dx = directions[i][0];
        int dy = directions[i][1];
        int x = col + dx;
        int y = row + dy;

        // Check if there is an opponent piece in the current direction
        if (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE && board[y][x] != player && board[y][x] != EMPTY) {
            // Move in the current direction until reaching the boundary or a player piece or an empty cell
            while (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE && board[y][x] != player && board[y][x] != EMPTY) {
                x += dx;
                y += dy;
            }

            // If a player piece is found, the move is valid
            if (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE && board[y][x] == player) {
                return true;
            }
        }
    }

    // If no valid move is found, the move is invalid
    return false;
}

/**
 * @brief Print the board with valid moves highlighted
 * 
 * @param player The player for whom valid moves are highlighted
 */
void print_highlighted_board(char player) {
    // Initialize a vector to store valid moves
    vector<pair<int, int>> valid_moves;

    // Iterate through all cells in the board and store valid moves
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == EMPTY && is_valid_move(i, j, player)) {
                valid_moves.push_back(make_pair(i, j));
            }
        }
    }

    // Highlight all valid moves
    for (int i = 0; i < valid_moves.size(); i++) {
        board[valid_moves[i].first][valid_moves[i].second] = VALID;
    }

    print_board();
    print_scores();

    // Dehighlight all valid moves
    for (int i = 0; i < valid_moves.size(); i++) {
        board[valid_moves[i].first][valid_moves[i].second] = EMPTY;
    }
}

/**
 * @brief Check if the game is over
 * 
 * @return A boolean indicating if the game is over
 */
bool is_game_over() {
    // If there's at least one empty cell with a valid move for either player, the game is not over
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == EMPTY && (is_valid_move(i, j, PLAYER1) || is_valid_move(i, j, PLAYER2))) {
                return false;
            }
        }
    }

    // If all cells are filled or no valid move exists for either player, the game is over
    return true;
}

/**
 * @brief Check if the current player's turn should be skipped
 * 
 * @param player The current player
 * @return A boolean indicating if the turn should be skipped
 */
bool turn_skip(char player) {
    // Iterate through all cells in the board
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            // In an empty cell if there is a valid move for player, the turn is not skipped
            if (board[i][j] == EMPTY && is_valid_move(i, j, player)) {
                return false;
            }
        }
    }

    // If no valid moves exist for player, the turn is skipped
    return true;
}


// AI FUNCTIONS

/**
 * @brief Determine the current game phase
 * 
 * Possible game phases:
 * 1. Opening: 4-20 pieces
 * 2. Midgame: 21-50 pieces
 * 3. Endgame: 51-64 pieces
 * 
 * @return An integer representing the current game phase
 */
int game_phase() {
    // Initialize a counter for the total number of pieces on the board
    int total_pieces = 0;

    // Iterate through all cells in the board and count the number of pieces on the board
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == PLAYER1 || board[i][j] == PLAYER2) {
                total_pieces++;
            }
        }
    }

    // Determine the game phase based on the total number of pieces
    switch (total_pieces) {
        case 4 ... 20:
            return 1;
        case 21 ... 50:
            return 2;
        case 51 ... 60:
            return 3;
        case 61 ... 64:
            return 4;
        default:
            return 0;
    }
}

/**
 * @brief Evaluate the board state for the current player
 * 
 * @param player The current player
 * @return An integer representing the evaluation score
 */
int evaluate_board(char player) {
    // Initialize counters
    int player1_score = 0;
    int player2_score = 0;
    int num_valid_moves = 0;

    // Determine the game phase
    int phase = game_phase();

    // Iterate through all cells in the board
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            // Count the number of valid moves for the opponent (for mobility)
            if (board[i][j] == EMPTY && is_valid_move(i, j, player)) {
                num_valid_moves++;
            }

            // Calculate the score for each player based on the position weights and game phase
            if (board[i][j] == PLAYER1) {
                switch (phase) {
                    case 1:
                        player1_score += POSITION_WEIGHTS[i][j];
                        // player2_score -= POSITION_WEIGHTS[i][j];
                        break;
                    case 2:
                        player1_score += POSITION_WEIGHTS[i][j];
                        // player2_score -= POSITION_WEIGHTS[i][j];
                        break;
                    case 3:
                        player1_score += ENDGAME_WEIGHTS[i][j];
                        // player2_score -= ENDGAME_WEIGHTS[i][j];
                        break;
                    case 4:
                        player1_score += 1;
                        // player2_score -= 1;
                        break;
                    default:
                        break;
                }
            } else if (board[i][j] == PLAYER2) {
                switch (phase) {
                    case 1:
                        player2_score += POSITION_WEIGHTS[i][j];
                        break;
                    case 2:
                        player2_score += POSITION_WEIGHTS[i][j];
                        break;
                    case 3:
                        player2_score += ENDGAME_WEIGHTS[i][j];
                        break;
                    case 4:
                        player2_score += 1;
                        break;
                    default:
                        break;
                }
            }
        }
    }

    // Calculate the score for the current player based on the number of pieces
    int player_score = (player == PLAYER1) ? player1_score - player2_score : player2_score - player1_score;

    // Calculate the evaluation score for the current player, taking mobility into account
    int evaluation_score = player_score + 10 * num_valid_moves;

    // Return the evaluation score for the current player
    return evaluation_score;
}

/**
 * @brief Negamax algorithm with alpha-beta pruning
 * 
 * @param depth The depth of the search tree
 * @param alpha The alpha value for alpha-beta pruning
 * @param beta The beta value for alpha-beta pruning
 * @param player The current player
 * @return An integer representing the evaluation score
 */
int negamax(int depth, int alpha, int beta, char player) {
    char opponent = get_opponent(player);

    // Check if the maximum depth is reached or the game is over
    if (depth == 0 || is_game_over()) {
        return evaluate_board(player);
    }

    // Initialize the best score
    int best_score = INT_MIN;

    // Iterate through all cells in the board
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            // Check if the move is valid
            if (is_valid_move(i, j, player)) {
                // Make a copy of the board
                vector<vector<char>> board_copy = board;

                // Make the move
                make_move(i, j, player);

                // Recursively update the score by calling negamax for the opponent
                int score = -negamax(depth - 1, -beta, -alpha, opponent);

                // Undo the move
                board = board_copy;

                // Update the best score
                best_score = max(best_score, score);
                alpha = max(alpha, score);

                // Perform alpha-beta pruning
                if (alpha >= beta) {
                    break;
                }
            }
        }
    }

    return best_score;
}

/**
 * @brief Predict the best move for the current player
 * 
 * @param player The current player
 * @return A pair of integers representing the row and column of the best move
 */
pair<int, int> predict_move(char player, int depth=DEFAULT_DEPTH) {
    char opponent = get_opponent(player);

    // Initialize the best move and best score
    pair<int, int> best_move;
    int best_score = INT_MIN;

    // Iterate through all cells in the board
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            // Check if the move is valid
            if (is_valid_move(i, j, player)) {
                // Make a copy of the board
                vector<vector<char>> board_copy = board;
                
                // Make the move
                make_move(i, j, player);

                // Call negamax to predict the score
                int score = -negamax(depth, INT_MIN, INT_MAX, opponent);

                // Undo the move
                board = board_copy;

                // Update the best move and best score
                if (score > best_score) {
                    best_score = score;
                    best_move = make_pair(i, j);
                }
            }
        }
    }

    return best_move;
}

/**
 * @brief Get the mode of the game from the user
 * 
 * Mode 1: Player vs Player
 * Mode 2: Player vs AI
 * Mode 3: AI vs AI
 * 
 * @return An integer representing the mode of the game
 */
int get_game_mode() {
    int mode;

    // Get the mode of the game from the user
    cout << "Enter the mode of the game:" << endl;
    cout << "1. Player vs Player" << endl;
    cout << "2. Player vs AI" << endl;
    cout << "3. AI vs AI" << endl;
    cin >> mode;

    // Validate the user input
    while (cin.fail() || mode < 1 || mode > 3) {
        cin.clear(); // Clear error flags
        cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignore rest of the line
        cout << "Please enter a valid game mode." << endl;
        cin >> mode;
    }
    cout << endl;

    return mode;
}

/**
 * @brief Get the depth of the search tree from the user
 * 
 * @return An integer representing the depth of the search tree
 */
int get_search_depth() {
    int depth;

    // Get the depth of the search tree from the user
    cout << "Enter the depth of the search tree:" << endl;
    cout << "If you would like to use the default search depth (" << DEFAULT_DEPTH << "), enter 0." << endl;
    cin >> depth;

    // Validate the user input
    while (depth < 0) {
        cout << "Please enter a valid depth." << endl;
        cin >> depth;
    }
    cout << endl;

    // Use the default search depth if the user entered 0
    if (depth == 0) {
        depth = DEFAULT_DEPTH;
    }

    return depth;
}

/**
 * @brief Get the disk color for the player from the user
 * 
 * @return A character representing the disk color
 */
char get_disk_color() {
    char disk_color;

    // Get the disk color for the player from the user
    cout << "Enter the disk color for the player (B/W):" << endl;
    cin >> disk_color;

    // Validate the user input
    while (disk_color != PLAYER1 && disk_color != PLAYER2) {
        cout << "Please enter a valid disk color (B/W):" << endl;
        cin >> disk_color;
    }
    cout << endl;

    return disk_color;
}


// MAIN FUNCTION

/**
 * @brief Othello game
 * 
 * @author Vishudh Shah
 * @since 2024-06-26
 */
int main() {
    pair<int, int> user_input;
    int row, col;
    int depth = DEFAULT_DEPTH;

    char player_color; // Player's disk color in Player vs AI mode

    // Get the game mode from the user
    int game_mode = get_game_mode();
    
    // If the game mode involves AI, get the search depth from the user
    if (game_mode == 2 || game_mode == 3) {
        depth = get_search_depth();

        // If the game mode is Player vs AI, get the disk color for the player from the user
        if (game_mode == 2) {
            player_color = get_disk_color();
        }
    }

    // Start with PLAYER1 (Black)
    char current_player = PLAYER1;

    // Initialize and print the starting board
    initialize_board();
    print_highlighted_board(current_player);
    cout << endl;

    // Game loop
    for (;;) {
        // Check if the game is over
        if (is_game_over()) {
            // Print the winning message
            print_winning_message();
            break;
        }

        // Check if the current player's turn should be skipped
        if (turn_skip(current_player)) {
            cout << "Player " << current_player << "'s turn is skipped!" << endl;
            switch_player(current_player);
            print_highlighted_board(current_player);
            continue;
        }

        // Count and print the move number
        static int move_number = 0;
        move_number++;
        cout << "Move " << move_number << endl;

        cout << "Player " << current_player << "'s turn." << endl;

        // Handle different game modes
        if (game_mode == 1 || (game_mode == 2 && current_player == player_color)) {
            // PvP or PvE (Player's turn)
            user_input = get_user_input();
            row = user_input.first;
            col = user_input.second;

            // Validate the user input
            while (!is_valid_move(row, col, current_player)) {
                cout << "Please see the valid moves highlighted!" << endl;
                user_input = get_user_input();
                row = user_input.first;
                col = user_input.second;
            }
            cout << endl;
        } else {
            // AI's turn
            pair<int, int> ai_move = predict_move(current_player, depth);
            row = ai_move.first;
            col = ai_move.second;

            // Convert row and col to othello notation
            char row_char = row + '1';
            char col_char = col + 'A';
            
            cout << "AI plays: " << string(1, col_char) + row_char << endl << endl;
        }

        // Make the move
        make_move(row, col, current_player);

        // Switch to the other player after the turn is complete
        switch_player(current_player);

        // Print the board with valid moves highlighted for the next player
        print_highlighted_board(current_player);
        cout << endl;
    }

    return 0;
}
