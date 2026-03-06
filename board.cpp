#include "board.hpp"
#include <iostream>
#include <format>
#include <fstream>
#include <ctime>
#include <chrono>
#include <filesystem>

std::string player_name(char player) {
    return player == PLAYER1 ? "Black" : "White";
}

void initialize_board() {
    board[3][3] = PLAYER2;
    board[3][4] = PLAYER1;
    board[4][3] = PLAYER1;
    board[4][4] = PLAYER2;
}

bool parse_fen(const std::string& fen) {
    // Split on '/' and validate exactly 8 rows
    std::vector<std::string> rows;
    std::string current;
    for (char c : fen) {
        if (c == '/') { rows.push_back(current); current.clear(); }
        else           { current += c; }
    }
    rows.push_back(current);

    if ((int)rows.size() != BOARD_SIZE) return false;

    for (int r = 0; r < BOARD_SIZE; r++) {
        int col = 0;
        for (char c : rows[r]) {
            if      (c == 'B' || c == 'b') { if (col >= BOARD_SIZE) return false; board[r][col++] = PLAYER1; }
            else if (c == 'W' || c == 'w') { if (col >= BOARD_SIZE) return false; board[r][col++] = PLAYER2; }
            else if (c >= '1' && c <= '8') { int n = c - '0'; if (col + n > BOARD_SIZE) return false;
                                             for (int k = 0; k < n; k++) board[r][col++] = EMPTY; }
            else return false; // unknown character
        }
        if (col != BOARD_SIZE) return false; // row didn't sum to 8
    }
    return true;
}

bool parse_64char(const std::string& s) {
    if ((int)s.length() != BOARD_SIZE * BOARD_SIZE) return false;

    for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
        char c = s[i];
        if      (c == 'B' || c == 'b') board[i / BOARD_SIZE][i % BOARD_SIZE] = PLAYER1;
        else if (c == 'W' || c == 'w') board[i / BOARD_SIZE][i % BOARD_SIZE] = PLAYER2;
        else if (c == '.')             board[i / BOARD_SIZE][i % BOARD_SIZE] = EMPTY;
        else return false; // unknown character
    }
    return true;
}

void print_board() {
    char column_labels[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'};

    std::cout << "  ";
    for (char label : column_labels) {
        std::cout << label << ' ';
    }
    std::cout << '\n';

    for (int i = 0; i < BOARD_SIZE; i++) {
        std::cout << i + 1 << ' ';
        for (int j = 0; j < BOARD_SIZE; j++) {
            std::cout << board[i][j] << ' ';
        }
        std::cout << '\n';
    }
}

void print_highlighted_board(char player) {
    // Initialize a vector to store valid moves
    std::vector<std::pair<int, int>> valid_moves;

    // Iterate through all cells in the board and store valid moves
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == EMPTY && is_valid_move(i, j, player)) {
                valid_moves.emplace_back(i, j);
            }
        }
    }

    // Highlight all valid moves
    for (const auto& move : valid_moves) {
        board[move.first][move.second] = VALID;
    }

    print_board();
    print_scores();

    // Dehighlight all valid moves
    for (const auto& move : valid_moves) {
        board[move.first][move.second] = EMPTY;
    }
}

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

std::pair<int, int> calculate_scores() {
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

    return std::make_pair(player1_score, player2_score);
}

void print_scores() {
    // Calculate the scores of both players
    std::pair<int, int> scores = calculate_scores();
    int player1_score = scores.first;
    int player2_score = scores.second;

    // Print the scores of both players
    std::cout << std::format("{}: {}, {}: {}\n", player_name(PLAYER1), player1_score, player_name(PLAYER2), player2_score);
}

void print_winning_message() {
    // Calculate how much did the player win by
    std::pair<int, int> scores = calculate_scores();
    int player1_score = scores.first;
    int player2_score = scores.second;
    int winner_score = (player1_score > player2_score) ? player1_score : player2_score;
    int loser_score = (player1_score < player2_score) ? player1_score : player2_score;
    int score_difference = winner_score - loser_score;

    std::cout << "Game Over.";
    if (score_difference == 0) {
        std::cout << " It's a tie!\n";
    } else {
        std::cout << std::format(" {} won by {} points!\n",
            player_name(player1_score > player2_score ? PLAYER1 : PLAYER2),
            score_difference);
    }
}

void export_game(const std::vector<std::pair<char, std::string>>& moves, int game_mode, char player_color, int time_limit_b, int time_limit_w, const std::string& start_pos) {
    // Build timestamped base filename inside games/ folder
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_info = std::localtime(&t);

    char timestamp[16];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);

    char date_str[20];
    std::strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M:%S", tm_info);

    std::filesystem::create_directories("games");
    std::string base = std::format("games/game_{}", timestamp);

    std::string mode_str = game_mode == 1 ? "Player vs Player"
                         : game_mode == 2 ? "Player vs AI"
                         : game_mode == 3 ? "AI vs AI"
                         :                  "Puzzle Mode";

    auto [b_score, w_score] = calculate_scores();

    // Raw sequence file: moves concatenated on one line
    std::ofstream raw(base + ".log");
    for (const auto& [player, move] : moves) raw << move;
    raw << '\n';

    // Human-readable file
    std::ofstream readable(base + ".txt");
    readable << std::format("Othello Game Log\nDate: {}\nMode: {}\n", date_str, mode_str);
    if (game_mode == 2)
        readable << std::format("Player: {}\nAI time limit: {}s\n", player_name(player_color), time_limit_b);
    else if (game_mode == 3)
        readable << std::format("Black AI time limit: {}s\nWhite AI time limit: {}s\n", time_limit_b, time_limit_w);
    else if (game_mode == 4)
        readable << std::format("Starting position: {}\nAI time limit: {}s\n", start_pos, time_limit_b);
    readable << '\n';

    for (int i = 0; i < (int)moves.size(); i++)
        readable << std::format("Move {:2}: {} ({})\n", i + 1, moves[i].second, player_name(moves[i].first));
    readable << std::format("\nBlack: {}, White: {}\n", b_score, w_score);
    if (b_score == w_score)
        readable << "Draw\n";
    else if (b_score > w_score)
        readable << std::format("Black won by {} points\n", b_score - w_score);
    else
        readable << std::format("White won by {} points\n", w_score - b_score);

    std::cout << std::format("Game saved to {}.log and {}.txt\n", base, base);
}