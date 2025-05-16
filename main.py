# Port from C++

# IMPORTS

import numpy as np
import re
import os


# GLOBAL VARIABLES

BOARD_SIZE = 8
EMPTY = '.'
VALID = '_'
PLAYER1 = 'B'
PLAYER2 = 'W'
DEFAULT_DEPTH = 5

board = np.full((BOARD_SIZE, BOARD_SIZE), EMPTY, dtype=str)

# Weighted piece counter for the evaluation function
POSITION_WEIGHTS = np.array([
    [100, -10, 20, 20, 20, 20, -10, 100],
    [-10, -20,  1,  1,  1,  1, -20, -10],
    [ 20,   1,  5,  5,  5,  5,   1,  20],
    [ 20,   1,  5,  5,  5,  5,   1,  20],
    [ 20,   1,  5,  5,  5,  5,   1,  20],
    [ 20,   1,  5,  5,  5,  5,   1,  20],
    [-10, -20,  1,  1,  1,  1, -20, -10],
    [100, -10, 20, 20, 20, 20, -10, 100]
])

ENDGAME_WEIGHTS = np.array([
    [100,   5, 10, 10, 10, 10,   5, 100],
    [  5,  -5,  1,  1,  1,  1,  -5,   5],
    [ 10,   1,  1,  1,  1,  1,   1,  10],
    [ 10,   1,  1,  1,  1,  1,   1,  10],
    [ 10,   1,  1,  1,  1,  1,   1,  10],
    [ 10,   1,  1,  1,  1,  1,   1,  10],
    [  5,  -5,  1,  1,  1,  1,  -5,   5],
    [100,   5, 10, 10, 10, 10,   5, 100]
])

# Last 10 moves uses a uniform weight matrix (because at this point all you care about is material, not position)


# FUNCTIONS

def initialize_board() -> None:
    """Initialize the starting board configuration"""
    global board
    board[3][3] = PLAYER2
    board[3][4] = PLAYER1
    board[4][3] = PLAYER1
    board[4][4] = PLAYER2

def print_board() -> None:
    """Print the board"""
    column_labels = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H']

    print(' ', end='')
    for label in column_labels:
        print(f' {label}', end='')
    print()
    for i in range(BOARD_SIZE):
        print(f'{i + 1}', end='')
        for j in range(BOARD_SIZE):
            print(f' {board[i][j]}', end='')
        print()

def get_user_input() -> tuple[int, int]:
    """
    Get user input for row and column
    Takes input in the format "A1" or "a1" and converts it to the corresponding row and column

    Returns:
        tuple[int, int]: The row and column
    """
    print("Enter your move (e.g. A1 or a1): ", end='')
    user_input = input().strip()

    # Validate the user input using regex
    while not re.match(r'^[A-Ha-h][1-8]$', user_input):
        print("Invalid input. Please enter a valid move (e.g. A1 or a1): ", end='')
        user_input = input().strip()
    
    # Convert the user input to row and column
    row = int(user_input[1]) - 1
    col = ord(user_input[0].upper()) - ord('A') 

    return row, col

def make_move(row: int, col: int, player: str) -> None:
    """
    Make a move on the board and flip the opponent's pieces

    Args:
        row (int): The row of the move
        col (int): The column of the move
        player (str): The player making the move ('B' or 'W')
    """
    global board
    board[row][col] = player

    # Check for opponent pieces in all eight directions
    directions = [(1, 0), (1, -1), (0, -1), (-1, -1), (-1, 0), (-1, 1), (0, 1), (1, 1)]
    for dx, dy in directions:
        x, y = col + dx, row + dy

        # Check if there is an opponent piece in the current direction
        if 0 <= x < BOARD_SIZE and 0 <= y < BOARD_SIZE and board[y, x] != player and board[y, x] != EMPTY:
            # Move in the current direction until reaching the boundary or a player piece or an empty cell
            while 0 <= x < BOARD_SIZE and 0 <= y < BOARD_SIZE and board[y, x] != player and board[y, x] != EMPTY:
                x += dx
                y += dy
            
            # Flip all opponent pieces in the current direction, going backwards
            if 0 <= x < BOARD_SIZE and 0 <= y < BOARD_SIZE and board[y, x] == player:
                x -= dx
                y -= dy
                while x != col or y != row:
                    board[y, x] = player
                    x -= dx
                    y -= dy

def switch_player(current_player: str) -> str:
    """
    Switch the current player

    Args:
        current_player (str): The current player ('B' or 'W')

    Returns:
        str: The next player ('B' or 'W')
    """
    return PLAYER2 if current_player == PLAYER1 else PLAYER1

def get_opponent(player: str) -> str:
    """
    Get the opponent's disk color

    Args:
        player (str): The current player ('B' or 'W')

    Returns:
        str: The opposing player ('B' or 'W')
    """
    return PLAYER2 if player == PLAYER1 else PLAYER1

def calculate_scores() -> tuple[int, int]:
    """
    Calculate the scores of both players

    Returns:
        tuple[int, int]: The scores for player 1 (B) and player 2 (W)
    """
    player1_score = np.sum(board == PLAYER1)
    player2_score = np.sum(board == PLAYER2)
    return player1_score, player2_score

def print_scores() -> None:
    """Print the scores of both players"""
    player1_score, player2_score = calculate_scores()
    print(f"Score: {PLAYER1} = {player1_score}, {PLAYER2} = {player2_score}")

def print_winning_message() -> None:
    """Print the winning message"""
    player1_score, player2_score = calculate_scores()
    
    print("Game Over.", end=" ")
    if player1_score == player2_score:
        print("It's a tie!")
    else:
        winner = PLAYER1 if player1_score > player2_score else PLAYER2
        score_difference = abs(player1_score - player2_score)
        print(f"Player {winner} won by {score_difference} points!")

def is_valid_move(row: int, col: int, player: str) -> bool:
    """
    Check if a move is valid
    
    Args:
        row: The row of the move
        col: The column of the move
        player: The player making the move
        
    Returns:
        A boolean indicating if the move is valid
    """
    # Check if the position is within bounds and the cell is empty
    if not (0 <= row < BOARD_SIZE and 0 <= col < BOARD_SIZE) or board[row, col] != EMPTY:
        return False

    # Check for opponent pieces in all eight directions
    # Create Cartesian coordinate system for directions and iterate through each direction
    directions = [(1, 0), (1, -1), (0, -1), (-1, -1), (-1, 0), (-1, 1), (0, 1), (1, 1)]
    for dx, dy in directions:
        x, y = col + dx, row + dy

        # Check if there is an opponent piece in the current direction
        if (0 <= x < BOARD_SIZE and 0 <= y < BOARD_SIZE and board[y, x] != player and board[y, x] != EMPTY):
            # Move in the current direction until reaching the boundary or a player piece or an empty cell
            while (0 <= x < BOARD_SIZE and 0 <= y < BOARD_SIZE and board[y, x] != player and board[y, x] != EMPTY):
                x += dx
                y += dy

            # If a player piece is found, the move is valid
            if (0 <= x < BOARD_SIZE and 0 <= y < BOARD_SIZE and board[y, x] == player):
                return True

    # If no valid move is found, the move is invalid
    return False

def clear_screen() -> None:
    """Clear the terminal screen"""
    os.system('cls' if os.name == 'nt' else 'clear')

def print_highlighted_board(player: str) -> None:
    """
    Print the board with valid moves highlighted
    
    Args:
        player: The player for whom valid moves are highlighted
    """
    global board

    # clear_screen()

    # Initialize a list to store valid moves
    valid_moves = []

    # Iterate through all cells in the board and store valid moves
    for i in range(BOARD_SIZE):
        for j in range(BOARD_SIZE):
            if board[i, j] == EMPTY and is_valid_move(i, j, player):
                valid_moves.append((i, j))

    # Highlight all valid moves
    for i in range(len(valid_moves)):
        board[valid_moves[i][0], valid_moves[i][1]] = VALID
    
    print_board()
    print_scores()

    # Dehighlight all valid moves
    for i in range(len(valid_moves)):
        board[valid_moves[i][0], valid_moves[i][1]] = EMPTY

def is_game_over() -> bool:
    """
    Check if the game is over
    
    Returns:
        A boolean indicating if the game is over
    """
    # If there's at least one empty cell with a valid move for either player, the game is not over
    for i in range(BOARD_SIZE):
        for j in range(BOARD_SIZE):
            if board[i, j] == EMPTY and (is_valid_move(i, j, PLAYER1) or is_valid_move(i, j, PLAYER2)):
                return False
    
    # If all cells are filled or no valid move exists for either player, the game is over
    return True

def turn_skip(player: str) -> bool:
    """
    Check if the current player's turn should be skipped
    
    Args:
        player: The current player
        
    Returns:
        A boolean indicating if the turn should be skipped
    """
    # Iterate through all cells in the board
    for i in range(BOARD_SIZE):
        for j in range(BOARD_SIZE):
            # In an empty cell if there is a valid move for player, the turn is not skipped
            if board[i, j] == EMPTY and is_valid_move(i, j, player):
                return False
    
    # If no valid moves exist for player, the turn is skipped
    return True


# AI FUNCTIONS

def game_phase() -> int:
    """
    Determine the current game phase

    Possible game phases:
    1. Opening: 4-20 pieces
    2. Midgame: 21-50 pieces
    3. Endgame: 51-60 pieces
    4. Final: 61-64 pieces

    Returns:
        An integer representing the current game phase
    """
    # Count total pieces on the board
    total_pieces = np.sum(board != EMPTY)

    # Determine the game phase based on the total number of pieces
    if 4 <= total_pieces <= 20:
        return 1
    elif 21 <= total_pieces <= 50:
        return 2
    elif 51 <= total_pieces <= 60:
        return 3
    elif 61 <= total_pieces <= 64:
        return 4
    else:
        return 0

def evaluate_board(player: str) -> int:
    """
    Evaluate the board state for the current player

    Args:
        player: The current player

    Returns:
        An integer representing the evaluation score
    """
    opponent = get_opponent(player)

    # Initialize counters
    player1_score = 0
    player2_score = 0

    # Determine the game phase
    phase = game_phase()

    # Use numpy operations to calculate scores more efficiently
    if phase == 1 or phase == 2:
        # For opening and midgame, use POSITION_WEIGHTS
        p1_positions = (board == PLAYER1)
        p2_positions = (board == PLAYER2)
        player1_score = np.sum(POSITION_WEIGHTS * p1_positions)
        player2_score = np.sum(POSITION_WEIGHTS * p2_positions)
    elif phase == 3:
        # For endgame, use ENDGAME_WEIGHTS
        p1_positions = (board == PLAYER1)
        p2_positions = (board == PLAYER2)
        player1_score = np.sum(ENDGAME_WEIGHTS * p1_positions)
        player2_score = np.sum(ENDGAME_WEIGHTS * p2_positions)
    elif phase == 4:
        # For final phase, just count pieces
        player1_score = np.sum(board == PLAYER1)
        player2_score = np.sum(board == PLAYER2)

    # Calculate the evaluation score for the current player
    return player1_score - player2_score if player == PLAYER1 else player2_score - player1_score

def negamax(depth: int, alpha: int, beta: int, player: str) -> int:
    """
    Negamax algorithm with alpha-beta pruning

    Args:
        depth: The depth of the search tree
        alpha: The alpha value for alpha-beta pruning
        beta: The beta value for alpha-beta pruning
        player: The current player

    Returns:
        An integer representing the evaluation score
    """
    global board
    opponent = get_opponent(player)

    # Check if the maximum depth is reached or the game is over
    if depth == 0 or is_game_over():
        return evaluate_board(player)

    # Initialize the best score
    best_score = float('-inf')

    # Iterate through all cells in the board
    for i in range(BOARD_SIZE):
        for j in range(BOARD_SIZE):
            # Check if the move is valid
            if is_valid_move(i, j, player):
                # Make a copy of the board
                board_copy = board.copy()

                # Make the move
                make_move(i, j, player)

                # Recursively update the score by calling negamax for the opponent
                score = -negamax(depth - 1, -beta, -alpha, opponent)

                # Undo the move
                board = board_copy

                # Update the best score
                best_score = max(best_score, score)
                alpha = max(alpha, score)

                # Perform alpha-beta pruning
                if alpha >= beta:
                    break

    # If no valid moves exist, pass the turn
    if best_score == float('-inf'):
        return -negamax(depth - 1, -beta, -alpha, opponent)

    return best_score

def predict_move(player: str, depth: int = DEFAULT_DEPTH) -> tuple[int, int]:
    """
    Predict the best move for the current player

    Args:
        player: The current player
        depth: The depth of the search tree

    Returns:
        A tuple of integers representing the row and column of the best move
    """
    global board
    opponent = get_opponent(player)

    # Initialize the best move and best score
    best_move = None
    best_score = float('-inf')

    # Iterate through all valid moves
    for i in range(BOARD_SIZE):
        for j in range(BOARD_SIZE):
            if is_valid_move(i, j, player):
                # Make a copy of the board
                board_copy = board.copy()

                # Make the move
                make_move(i, j, player)

                # Call negamax to predict the score
                score = -negamax(depth, float('-inf'), float('inf'), opponent)

                # Undo the move
                board = board_copy

                # Update the best move and best score
                if score > best_score:
                    best_score = score
                    best_move = (i, j)

    return best_move

def get_game_mode() -> int:
    """
    Get the mode of the game from the user
    
    Mode 1: Player vs Player
    Mode 2: Player vs AI
    Mode 3: AI vs AI
    
    Returns:
        An integer representing the mode of the game
    """
    print("1. Player vs Player")
    print("2. Player vs AI")
    print("3. AI vs AI")
    print("Enter the mode of the game: ", end='')

    while True:
        try:
            # Get the mode of the game from the user
            mode = int(input().strip())

            # Validate the user input
            if mode < 1 or mode > 3:
                raise ValueError("Game mode must be between 1 and 3")

            print()
            return mode

        except ValueError:
            print("Please enter a valid game mode: ", end='')

def get_search_depth() -> int:
    """
    Get the depth of the search tree from the user
    
    Returns:
        An integer representing the depth of the search tree
    """
    print("Enter the depth of the search tree:")
    print(f"If you would like to use the default search depth ({DEFAULT_DEPTH}), enter 0 or simply hit enter.")

    while True:
        try:
            # Get the depth of the search tree from the user
            depth = input().strip()

            # Use the default search depth if the user entered 0 or hit enter
            if depth == '0' or depth == '':
                depth = DEFAULT_DEPTH
            else:
                depth = int(depth)

            # Validate the user input
            if depth < 0:
                raise ValueError("Depth must be non-negative")

            print()
            return depth

        except ValueError:
            print("Please enter a valid depth.")

def get_disk_color() -> str:
    """
    Get the disk color for the player from the user

    Returns:
        A character representing the disk color
    """
    # Get the disk color for the player from the user
    disk_color = input("Enter the disk color for the player (B/W): ").strip().upper()

    # Validate the user input
    while disk_color not in [PLAYER1, PLAYER2]:
        disk_color = input("Please enter a valid disk color (B/W): ").strip().upper()
    
    print()
    return disk_color


# MAIN GAME LOOP

if __name__ == "__main__":
    # Initialize variables
    row, col = -1, -1
    depth = DEFAULT_DEPTH
    player_color = None  # Player's disk color in Player vs AI mode

    # Get the game mode from the user
    game_mode = get_game_mode()

    # If the game mode involves AI, get the search depth from the user
    if game_mode == 2 or game_mode == 3:
        depth = get_search_depth()

        # If the game mode is Player vs AI, get the disk color for the player from the user
        if game_mode == 2:
            player_color = get_disk_color()

    # Start with PLAYER1 (Black)
    current_player = PLAYER1

    # Initialize and print the starting board
    initialize_board()
    print_highlighted_board(current_player)
    print()

    # Game loop
    move_number = 0  # Initialize move counter

    while True:
        # Check if the game is over
        if is_game_over():
            # Print the winning message
            print_winning_message()
            break

        # Check if the current player's turn should be skipped
        if turn_skip(current_player):
            print(f"Player {current_player}'s turn is skipped!")
            current_player = switch_player(current_player)
            print_highlighted_board(current_player)
            continue

        # Count and print the move number
        move_number += 1
        print(f"Move {move_number}")

        print(f"Player {current_player}'s turn.")

        # Handle different game modes
        if game_mode == 1 or (game_mode == 2 and current_player == player_color):
            # PvP or PvE (Player's turn)
            row, col = get_user_input()

            # Validate the user input
            while not is_valid_move(row, col, current_player):
                print("Please see the valid moves highlighted!")
                row, col = get_user_input()
            print()
        else:
            # AI's turn
            row, col = predict_move(current_player, depth)

            # Convert row and col to othello notation
            row_char = str(row + 1)
            col_char = chr(col + ord('A'))

            print(f"AI plays: {col_char + row_char}")
            print()

        # Make the move
        make_move(row, col, current_player)

        # Switch to the other player after the turn is complete
        current_player = switch_player(current_player)

        # Print the board with valid moves highlighted for the next player
        print_highlighted_board(current_player)
        print()
