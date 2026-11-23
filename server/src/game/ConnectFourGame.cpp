#include "ConnectFourGame.h"

ConnectFourGame::ConnectFourGame() {
    reset(PLAYER_1);
}

void ConnectFourGame::reset(char starting_player) {
    for (auto& row : board) {
        row.fill('.');
    }
    current_turn = starting_player;
}

MoveResult ConnectFourGame::drop_disc(int col, char player) {
    if (player != current_turn)
        return MoveResult::NOT_YOUR_TURN;

    if (col < 0 || col >= COLS)
        return MoveResult::OUT_OF_BOUNDS;

    int target_row = -1;
    for (int r = ROWS - 1; r >= 0; r--) {
        if (board[r][col] == EMPTY) {
            target_row = r;
            break;
        }
    }
    if (target_row == -1)
        return MoveResult::COLUMN_FULL;

    board[target_row][col] = player;

    if (check_win(target_row, col))
        return MoveResult::WIN;
    
    if (is_board_full())
        return MoveResult::DRAW;

    current_turn = (current_turn == PLAYER_1) ? PLAYER_2 : PLAYER_1;
    return MoveResult::OK;
}

std::string ConnectFourGame::serialize_board() const {
    std::string result;
    result.reserve(ROWS * COLS);

    for (const auto& row : board) {
        for (const char c : row) {
            result.push_back(c);
        }
    }
    return result;
}

bool ConnectFourGame::check_win(int last_row, int last_col) const {
    const char player = board[last_row][last_col];
    constexpr int directions[4][2] = {
        {0, 1}, // vodorovne
        {1, 0}, // svisle
        {1, 1}, // hlavni diag
        {-1, -1} // vedlejsi diag
    };

    for (const auto& dir : directions) {
        int d_row = dir[0];
        int d_col = dir[1];

        const int total = 1 + count_in_direction(last_row, last_col, d_row, d_col, player) + count_in_direction(last_row, last_col, -d_row, -d_col, player);
        if (total >= 4)
            return true;
    }

    return false;
}

int ConnectFourGame::count_in_direction(int row, int col, int d_row, int d_col, char player) const {
    int count = 0;
    int r = row + d_row;
    int c = col + d_col;

    while (r >= 0 && r < ROWS && c >= 0 && c < COLS && board[r][c] == player) {
        count++;
        r += d_row;
        c += d_col;
    }

    return count;
}

bool ConnectFourGame::is_board_full() const {
    for (int c = 0; c < COLS; c++) {
        // staci kontrolovat horni radek, protoze kameny padaji
        if (board[0][c] == EMPTY)
            return false;
    }
    return true;
}

char ConnectFourGame::get_current_turn() const noexcept {
    return current_turn;
}