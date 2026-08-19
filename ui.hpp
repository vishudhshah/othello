#pragma once

#include <string>

// Lifecycle
void ui_init();
void ui_teardown();

// Rendering
void render_game_screen(char current_player, const std::string& status_line = "");
void render_status_message(const std::string& msg);
// Returns true if the player chose to start a new game, false to exit.
bool render_winning_screen(char resigned_by = '\0');

// Persistent move/skip log shown below the board
void log_move(const std::string& text);
void clear_move_log();
// Scrolls the visible move-log window by `delta` entries (positive = older) and redraws it.
void scroll_move_log(int delta);

// Discard any input queued up while the AI (or another non-input phase) was busy,
// so a click made during that time is never played as an unintended "pre-move".
void discard_pending_input();
