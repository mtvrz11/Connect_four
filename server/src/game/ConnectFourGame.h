#pragma once

#include <array>
#include <cstddef>
#include <string>

enum class MoveResult {
    OK, // tah probehl, hra pokracuje
    WIN, // hrac vyhral
    DRAW, // remiza
    NOT_YOUR_TURN, // hrac se pokousi tahnout, i kdyz neni na tahu
    OUT_OF_BOUNDS, // tah mimo hraci desku
    COLUMN_FULL // tah do plneho sloupce
};

class ConnectFourGame {
public:
    static constexpr int ROWS = 6;
    static constexpr int COLS = 7;
    static constexpr char EMPTY = '.';
    static constexpr char PLAYER_1 = 'R';
    static constexpr char PLAYER_2 = 'B';

    ConnectFourGame();

    void reset(char starting_player = PLAYER_1);

    MoveResult drop_disc(int col, char player);

    char get_current_turn() const noexcept;

    std::string serialize_board() const;

private:
    bool check_win(int last_row, int last_col) const;
    int count_in_direction(int row, int col, int d_row, int d_col, char player) const;
    bool is_board_full() const;

    std::array<std::array<char, COLS>, ROWS> board{};
    char current_turn = PLAYER_1;
};