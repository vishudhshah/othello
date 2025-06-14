#include "Player.hpp"

Player::Player(char symbol) : playerSymbol(symbol) {}

char Player::getSymbol() const {
    return playerSymbol;
}
