#include "chess.h"

#define DEFAULT_DEPTH 3
#define MAX_DEPTH 5

//#define AUTOMATCH
#define UCI
#ifdef TEST_FEATURE
#define CHECK_EXTENSIONS
#endif

#if defined(AUTOMATCH) && defined(UCI)
#error stupid
#endif

static const int piece_values[] = { 0,
                                    100, /* pawn */
                                    500, /* rook */
                                    320, /* knight */
                                    330, /* bishop */
                                    10000,
                                    0  /* king, value doesn't matter */
};

int count_material(const struct chess_ctx *ctx, int color)
{
    int total = 0;
    for(int y = 0; y < 8; ++y)
    {
        for(int x = 0; x < 8; ++x)
        {
            if(ctx->board[y][x].color == color)
            {
                total += piece_values[ctx->board[y][x].type];
                total += location_bonuses[ctx->board[y][x].type - 1][color == WHITE ? y : 7 - y][x];

#if 0
                /* pawn near promotion */
                if(ctx->board[y][x].type == PAWN)
                {
                    if((y >= 5 && ctx->board[y][x].color == WHITE) ||
                       (y <= 2 && ctx->board[y][x].color == BLACK))
                        total += 4;

                    /* pawns are more valuable the further they have advanced */
                    total += ctx->board[y][x].color == WHITE ? y : 7 - y;
                }
                if(ctx->board[y][x].type == KING && (y == 2 || y == 6)) /* castled king */
                    total += 20;
#endif
            }
        }
    }
    //printf("color %d has %d material\n", color, total);
    return total;
}

bool count_space_cb(void *data, const struct chess_ctx *ctx, struct move_t move)
{
    int *count = data;
    (*count)++;
    if(move.type == NORMAL)
    {
        int x = move.data.normal.to.x, y = move.data.normal.to.y;
        if(ctx->board[y][x].color != NONE) /* threatens an enemy piece */
            (*count)++;
#if 0
        if((x == 3 || x == 4) && (y == 3 || y == 4)) /* controls center */
            (*count)++;
#endif
    }
    return true;
}

/* essentially returns the total of the number of squares each piece
 * can move to */
int count_space(const struct chess_ctx *ctx, int color)
{
    int space = 0;
    for(int y = 0; y < 8; ++y)
    {
        for(int x = 0; x < 8; ++x)
        {
            if(ctx->board[y][x].color == color)
            {
                for_each_move(ctx, y, x, count_space_cb, &space, true, true);
            }
        }
    }
    //printf("color %d has %d space\n", color, space);
    return space;
}

#define inv_player(p) ((p)==WHITE?BLACK:WHITE)

bool king_in_checkmate(const struct chess_ctx *ctx, int color)
{
    struct coordinates king;
    if(king_in_check(ctx, color, &king))
    {
        /* check that there are no legal moves */
        return count_space(ctx, color) == 0;
    }
    return false;
}

int eval_position(const struct chess_ctx *ctx, int color)
{
    int score = 0;

//    score += count_material(ctx, color) * 4;
//    score -= count_material(ctx, inv_player(color)) * 2;
    score += count_material(ctx, color);
    score -= count_material(ctx, inv_player(color));
    score += count_space(ctx, color);
    score -= count_space(ctx, inv_player(color));

#if 0
    if(can_castle(ctx, color, QUEENSIDE) || can_castle(ctx, color, KINGSIDE))
        score += 25;
#endif

    if(king_in_check(ctx, color, NULL))
    {
#ifdef CHECK_PENALTIES
        score -= 9;
#endif
        if(king_in_checkmate(ctx, color))
            score -= 200000;
    }
    else if(king_in_check(ctx, inv_player(color), NULL))
    {
#ifdef CHECK_PENALTIES
        score += 5;
#endif
        if(king_in_checkmate(ctx, inv_player(color)))
            score += 200000;
    }

    return score;
}

#define valid_coords(y, x) ((0 <= y && y <= 7) && (0 <= x && x <= 7))

int location_bonuses[6][8][8];

static const int location_bonuses_early[6][8][8] =  {
    {
        // Pawns - early/mid
        //  A   B   C   D   E   F   G   H
        { 0,  0,  0,  0,  0,  0,  0,  0 }, // 1
        { -1, -7,-11,-35,-13,  5,  3, -5 }, // 2
        { 1,  1, -6,-19, -6, -7, -4, 10 }, // 3
        { 1, 14,  8,  4,  5,  4, 10,  7 }, // 4
        { 9, 30, 23, 31, 31, 23, 17, 11 }, // 5
        { 21, 54, 72, 56, 77, 95, 71, 11 }, // 6
        { 118,121,173,168,107, 82,-16, 22 }, // 7
        { 0,  0,  0,  0,  0,  0,  0,  0 } // 8
    },
    {
        // Rooks - early/mid
        //  A   B   C   D   E   F   G   H
        { -2, -1,  3,  1,  2,  1,  4, -8 }, // 1
        { -26, -6,  2, -2,  2,-10, -1,-29 }, // 2
        { -16,  0,  3, -3,  8, -1, 12,  3 }, // 3
        { -9, -5,  8, 14, 18,-17, 13,-13 }, // 4
        { 19, 33, 46, 57, 53, 39, 53, 16 }, // 5
        { 24, 83, 54, 75,134,144, 85, 75 }, // 6
        { 46, 33, 64, 62, 91, 89, 70,104 }, // 7
        { 84,  0,  0, 37,124,  0,  0,153 }  // 8
    },
    {
        // Knights - early/mid
        //  A   B   C   D   E   F   G   H
        { -99,-30,-66,-64,-29,-19,-61,-81 }, // 1
        { -56,-31,-28, -1, -7,-20,-42,-11 }, // 2
        { -38,-16,  0, 14,  8,  3,  3,-42 }, // 3
        { -14,  0,  2,  3, 19, 12, 33, -7 }, // 4
        { -14, -4, 25, 33, 10, 33, 14, 43 }, // 5
        { -22, 18, 60, 64,124,143, 55,  6 }, // 6
        { -34, 24, 54, 74, 60,122,  2, 29 }, // 7
        { -60,  0,  0,  0,  0,  0,  0,  0 } // 8
    },{
        // Bishops - early/mid
        //  A   B   C   D   E   F   G   H
        { -7, 12, -8,-37,-31, -8,-45,-67 }, // 1
        { 15,  5, 13,-10,  1,  2,  0, 15 }, // 2
        { 5, 12, 14, 13, 10, -1,  3,  4 }, // 3
        { 1,  5, 23, 32, 21,  8, 17,  4 }, // 4
        { -1, 16, 29, 27, 37, 27, 17,  4 }, // 5
        { 7, 27, 20, 56, 91,108, 53, 44 }, // 6
        { -24,-23, 30, 58, 65, 61, 69, 11 }, // 7
        { 0,  0,  0,  0,  0,  0,  0,  0 } // 8
    },{
        // Queens - early/mid
        //  A   B   C   D   E   F   G   H
        { 1,-10,-11,  3,-15,-51,-83,-13 }, // 1
        { -7,  3,  2,  5, -1,-10, -7, -2 }, // 2
        { -11,  0, 12,  2,  8, 11,  7, -6 }, // 3
        { -9,  5,  7,  9, 18, 17, 26,  4 }, // 4
        { -6,  0, 15, 25, 32,  9, 26, 12 }, // 5
        { -16, 10, 13, 25, 37, 30, 15, 26 }, // 6
        { 1, 11, 35,  0, 16, 55, 39, 57 }, // 7
        { -13,  6,-42,  0, 29,  0,  0,102 }  // 8
    },{
        // Kings - early/mid
        //  A   B   C   D   E   F   G   H
        { 0,  0,  0, -9,  0, -9, 25,  0 }, // 1
        { -9, -9, -9, -9, -9, -9, -9, -9 }, // 2
        { -9, -9, -9, -9, -9, -9, -9, -9 }, // 3
        { -9, -9, -9, -9, -9, -9, -9, -9 }, // 4
        { -9, -9, -9, -9, -9, -9, -9, -9 }, // 5
        { -9, -9, -9, -9, -9, -9, -9, -9 }, // 6
        { -9, -9, -9, -9, -9, -9, -9, -9 }, // 7
        { -9, -9, -9, -9, -9, -9, -9, -9 } // 8
    }
};

static const int location_bonuses_endgame[6][8][8] = {
    // Pawns - endgame
    { //  A   B   C   D   E   F   G   H
        { 0,  0,  0,  0,  0,  0,  0,  0 }, // 1
        { -17,-17,-17,-17,-17,-17,-17,-17 }, // 2
        { -11,-11,-11,-11,-11,-11,-11,-11 }, // 3
        { -7, -7, -7, -7, -7, -7, -7, -7 }, // 4
        { 16, 16, 16, 16, 16, 16, 16, 16 }, // 5
        { 55, 55, 55, 55, 55, 55, 55, 55 }, // 6
        { 82, 82, 82, 82, 82, 82, 82, 82 }, // 7
        { 0,  0,  0,  0,  0,  0,  0,  0 }, // 8
    },
    // Knights - endgame
    { //  A   B   C   D   E   F   G   H
        { -99,-99,-94,-88,-88,-94,-99,-99 }, // 1
        { -81,-62,-49,-43,-43,-49,-62,-81 }, // 2
        { -46,-27,-15, -9, -9,-15,-27,-46 }, // 3
        { -22, -3, 10, 16, 16, 10, -3,-22 }, // 4
        { -7, 12, 25, 31, 31, 25, 12, -7 }, // 5
        { -2, 17, 30, 36, 36, 30, 17, -2 }, // 6
        { -7, 12, 25, 31, 31, 25, 12, -7 }, // 7
        { -21, -3, 10, 16, 16, 10, -3,-21 },  // 8
    },
    // Bishops - endgame
    {   //  A   B   C   D   E   F   G   H
        { -27,-21,-17,-15,-15,-17,-21,-27 }, // 1
        { -10, -4,  0,  2,  2,  0, -4,-10 }, // 2
        { 2,  8, 12, 14, 14, 12,  8,  2 }, // 3
        { 11, 17, 21, 23, 23, 21, 17, 11 }, // 4
        { 14, 20, 24, 26, 26, 24, 20, 14 }, // 5
        { 13, 19, 23, 25, 25, 23, 19, 13 }, // 6
        { 8, 14, 18, 20, 20, 18, 14,  8 }, // 7
        { -2,  4,  8, 10, 10,  8,  4, -2 },  // 8
    },{
        // Rooks - endgame
        //  A   B   C   D   E   F   G   H
        { -32,-31,-30,-29,-29,-30,-31,-32 }, // 1
        { -27,-25,-24,-24,-24,-24,-25,-27 }, // 2
        { -15,-13,-12,-12,-12,-12,-13,-15 }, // 3
        { 1,  2,  3,  4,  4,  3,  2,  1 }, // 4
        { 15, 17, 18, 18, 18, 18, 17, 15 }, // 5
        { 25, 27, 28, 28, 28, 28, 27, 25 }, // 6
        { 27, 28, 29, 30, 30, 29, 28, 27 }, // 7
        { 16, 17, 18, 19, 19, 18, 17, 16 },  // 8
    },{
        // Queens - endgame
        //  A   B   C   D   E   F   G   H
        { -61,-55,-52,-50,-50,-52,-55,-61 }, // 1
        { -31,-26,-22,-21,-21,-22,-26,-31 }, // 2
        { -8, -3,  1,  3,  3,  1, -3, -8 }, // 3
        { 9, 14, 17, 19, 19, 17, 14,  9 }, // 4
        { 19, 24, 28, 30, 30, 28, 24, 19 }, // 5
        { 23, 28, 32, 34, 34, 32, 28, 23 }, // 6
        { 21, 26, 30, 31, 31, 30, 26, 21 }, // 7
        { 12, 17, 21, 23, 23, 21, 17, 12 },  // 8
    },{
        // Kings - endgame
        //  A   B   C   D   E   F   G   H
        { -34,-30,-28,-27,-27,-28,-30,-34 }, // 1
        { -17,-13,-11,-10,-10,-11,-13,-17 }, // 2
        { -2,  2,  4,  5,  5,  4,  2, -2 }, // 3
        { 11, 15, 17, 18, 18, 17, 15, 11 }, // 4
        { 22, 26, 28, 29, 29, 28, 26, 22 }, // 5
        { 31, 34, 37, 38, 38, 37, 34, 31 }, // 6
        { 38, 41, 44, 45, 45, 44, 41, 38 }, // 7
        { 42, 46, 48, 50, 50, 48, 46, 42 },  // 8
    }
};

const struct coordinates king_moves[] = {
    { 0, 1 },
    { 1, 1 },
    { 1, 0 },
    { 1, -1 },
    { 0, -1 },
    { -1, -1 },
    { -1, 0 },
    { -1, 1 },
    { COORD_END, COORD_END },
};

const struct coordinates rook_directions[] = {
    { 0, 1 },
    { 1, 0 },
    { 0, -1 },
    { -1, 0 },
    { COORD_END, COORD_END },
};

const struct coordinates queen_directions[] = {
    { 0, 1 },
    { 1, 1 },
    { 1, 0 },
    { 1, -1 },
    { 0, -1 },
    { -1, -1 },
    { -1, 0 },
    { -1, 1 },
    { COORD_END, COORD_END },
};

const struct coordinates bishop_directions[] = {
    { 1, 1 },
    { 1, -1 },
    { -1, -1 },
    { -1, 1 },
    { COORD_END, COORD_END },
};

const struct coordinates knight_moves[] = {
    { 1, 2 },
    { 2, 1 },
    { 2, -1 },
    { 1, -2 },
    { -1, 2 },
    { -2, 1 },
    { -2, -1 },
    { -1, -2 },
    { COORD_END, COORD_END }
};

const struct coordinates *line_moves[] = {
    NULL,
    NULL,
    rook_directions,
    NULL,
    bishop_directions,
    queen_directions,
    NULL
};

const struct coordinates *jump_moves[] = {
    NULL,
    NULL,
    NULL,
    knight_moves,
    NULL,
    NULL,
    king_moves,
};

#define friendly_occupied(ctx, y, x, c) ((ctx)->board[(y)][(x)].color == NONE ? false : ((ctx)->board[(y)][(x)].color == (c)))

#if 0
bool friendly_occupied(const struct chess_ctx *ctx, int y, int x, int color)
{
    if(!valid_coords(y, x))
        return false;
    const struct piece_t *piece = &ctx->board[y][x];
    if(piece->type == EMPTY)
        return false;
    switch(piece->color)
    {
    case NONE:
        return false;
    case WHITE:
        return color == WHITE;
    case BLACK:
        return color == BLACK;
    default:
        assert(false);
    }
}
#endif

#define enemy_occupied(ctx, y, x, c) (valid_coords((y),(x))?((ctx)->board[(y)][(x)].color == NONE ? false : ((ctx)->board[(y)][(x)].color == inv_player(c))):false)

#if 0
bool enemy_occupied(const struct chess_ctx *ctx, int y, int x, int color)
{
    if(!valid_coords(y, x))
        return false;
    const struct piece_t *piece = &ctx->board[y][x];
    if(piece->type == EMPTY)
        return false;
    switch(piece->color)
    {
    case NONE:
        return false;
    case WHITE:
        return color == BLACK;
    case BLACK:
        return color == WHITE;
    default:
        assert(false);
    }
}
#endif

#define occupied(ctx, y, x) (valid_coords((y),(x))?((ctx)->board[(y)][(x)].color != NONE):false)

struct check_info {
    int color;
    bool checked;
    struct coordinates king;
};

bool detect_check_cb(void *data, const struct chess_ctx *ctx, struct move_t move)
{
    struct check_info *info = data;
    int x, y;
    if(move.type == NORMAL)
    {
        x = move.data.normal.to.x;
        y = move.data.normal.to.y;
    }
    else if(move.type == PROMOTION)
    {
        x = move.data.promotion.to.x;
        y = move.data.promotion.to.y;
    }
    else
        return true;

    if(ctx->board[y][x].type == KING && ctx->board[y][x].color == info->color)
    {
        info->king.y = y;
        info->king.x = x;
        info->checked = true;
        return false;
    }
    return true;
}

bool king_in_check(const struct chess_ctx *ctx, int color, struct coordinates *king)
{
    struct check_info info;
    info.color = color;
    info.checked = false;
    for(int y = 0; y < 8; ++y)
    {
        for(int x = 0; x < 8; ++x)
        {
            /* check enemy pieces */
            if(ctx->board[y][x].color == inv_player(color))
            {
                for_each_move(ctx, y, x, detect_check_cb, &info, false, false);
                if(info.checked)
                    goto early;
            }
        }
    }
early:
    if(info.checked)
    {
        if(king)
            *king = info.king;
    }
    return info.checked;
}

struct threatened_info {
    bool threatened;
    int y, x;
};

bool threatened_cb(void *data, const struct chess_ctx *ctx, struct move_t move)
{
    (void) ctx;
    struct threatened_info *info = data;

    int x, y;
    if(move.type == NORMAL)
    {
        x = move.data.normal.to.x;
        y = move.data.normal.to.y;
    }
    else if(move.type == PROMOTION)
    {
        x = move.data.promotion.to.x;
        y = move.data.promotion.to.y;
    }
    else
        return true;

    if(y == info->y && x == info->x)
    {
        info->threatened = true;
        return false;
    }
    return true;
}

/* color is friendly */
/* checks whether enemy threatens a square */
bool square_threatened(const struct chess_ctx *ctx, int ty, int tx, int color)
{
    struct threatened_info info;
    info.threatened = false;
    info.y = ty;
    info.x = tx;

    for(int y = 0; y < 8; ++y)
    {
        for(int x = 0; x < 8; ++x)
        {
            /* check enemy pieces */
            if(ctx->board[y][x].color == inv_player(color))
            {
                for_each_move(ctx, y, x, threatened_cb, &info, false, false);
                if(info.threatened)
                    return true;
            }
        }
    }
    return false;
}

struct move_t construct_move(int color, int y, int x, int dy, int dx)
{
    struct move_t ret;
    ret.color = color;
    ret.type = NORMAL;
    ret.data.normal.from = (struct coordinates) { y, x };
    ret.data.normal.to = (struct coordinates) { y + dy, x + dx };
    return ret;
}

inline bool gen_and_call(const struct chess_ctx *ctx,
                         int y, int x,
                         int dy, int dx,
                         bool (*cb)(void *data, const struct chess_ctx*, struct move_t),
                         void *data, bool enforce_check)
{
    struct move_t move = construct_move(ctx->board[y][x].color,
                                        y, x,
                                        dy, dx);

    bool promotion = false;
    if(ctx->board[y][x].type == PAWN &&
       (y + dy == 0 || y + dy == 7))
        promotion = true;

    /* move is to castle, must be before next if(), as it relies on it
     * to make sure it's legal */
    if(ctx->board[y][x].type == KING && ABS(dx) == 2)
    {
        move.type = CASTLE;
        move.data.castle_style = dx > 0 ? KINGSIDE : QUEENSIDE;
    }

    if(enforce_check)
    {
        struct chess_ctx local = *ctx;
        execute_move(&local, move);
        bool check_after = king_in_check(&local, ctx->board[y][x].color, NULL);

        /* move puts player in check */
        if(check_after)
            return true;
    }

    if(promotion)
    {
        move.type = PROMOTION;
        move.data.promotion.from = (struct coordinates) { y, x  };
        move.data.promotion.to = (struct coordinates) { y + dy, x + dx };

        /* try all possible pieces */
        enum piece promote_pieces[] = { ROOK, KNIGHT, BISHOP, QUEEN };
        for(unsigned int i = 0; i < ARRAYLEN(promote_pieces); ++i)
        {
            move.data.promotion.type = promote_pieces[i];
            if(!cb(data, ctx, move))
                return false;
        }
        return true;
    }
    else
    {
        return cb(data, ctx, move);
    }
}

void for_each_move(const struct chess_ctx *ctx,
                   int y, int x,
                   bool (*cb)(void *data, const struct chess_ctx*, struct move_t),
                   void *data, bool enforce_check, bool consider_castle)
{
    assert(valid_coords(y, x));

    const struct piece_t *piece = &ctx->board[y][x];

    switch(piece->type)
    {
    case EMPTY:
        return;
    case PAWN:
    {
        /* special case */
        int dy = (piece->color == WHITE ? 1 : -1);

        if(!valid_coords(y + dy, x))
            break;

        /* en passant */
        if((piece->color == WHITE && y == 4) ||
           (piece->color == BLACK && y == 3))
        {
            /* piece to right */
            int opp = piece->color == WHITE ? 1 : 0;
            if(valid_coords(y + dy, x + 1) && ctx->en_passant[opp][x + 1])
            {
                if(!gen_and_call(ctx, y, x, dy, 1, cb, data, enforce_check))
                    return;
            }
            if(valid_coords(y + dy, x - 1) && ctx->en_passant[opp][x - 1])
            {
                if(!gen_and_call(ctx, y, x, dy, -1, cb, data, enforce_check))
                    return;
            }
        }

        /* 2 squares on first move */
        if((ctx->board[y][x].color == WHITE && y == 1) ||
           (ctx->board[y][x].color == BLACK && y == 6))
            if(!occupied(ctx, y + 2 * dy, x) && !occupied(ctx, y + dy, x))
                if(!gen_and_call(ctx, y, x, 2 * dy, 0, cb, data, enforce_check))
                    return;

        for(int dx = -1; dx <= 1; dx += 2)
        {
            if(enemy_occupied(ctx, y + dy, x + dx, piece->color))
            {
                if(!gen_and_call(ctx, y, x, dy, dx, cb, data, enforce_check))
                    return;
            }
        }

        if(!occupied(ctx, y + dy, x))
            if(!gen_and_call(ctx, y, x, dy, 0, cb, data, enforce_check))
                return;
        break;
    }
    case ROOK:
    case BISHOP:
    case QUEEN:
    {
        const struct coordinates *dir = line_moves[piece->type];
        while(dir->x != COORD_END)
        {
            bool clear = true;
            for(int i = 1; i < 8 && clear; ++i)
            {
                struct coordinates d = { i * dir->y, i * dir->x };
                clear = valid_coords(y + d.y, x + d.x);
                if(!clear)
                    break;
                if(!occupied(ctx, y + d.y, x + d.x))
                {
                    if(!gen_and_call(ctx, y, x, d.y, d.x, cb, data, enforce_check))
                        return;
                }
                else
                {
                    /* occupied square */
                    if(enemy_occupied(ctx, y + d.y, x + d.x, piece->color))
                        if(!gen_and_call(ctx, y, x, d.y, d.x, cb, data, enforce_check))
                            return;
                    clear = false;
                }
            }
            ++dir;
        }
        break;
    }
    case KNIGHT:
    case KING:
    {
        const struct coordinates *moves = jump_moves[piece->type];

        /* castling */
        if(piece->type == KING && consider_castle)
        {
            if(x == 4 && (y == 0 || y == 7))
            {
                if(can_castle(ctx, piece->color, QUEENSIDE))
                    if(!gen_and_call(ctx, y, x, 0, -2, cb, data, false))
                        return;
                if(can_castle(ctx, piece->color, KINGSIDE))
                    if(!gen_and_call(ctx, y, x, 0, 2, cb, data, false))
                        return;
            }
        }

        while(moves->x != COORD_END)
        {
            struct coordinates d = *moves;
            if(valid_coords(y + d.y, x + d.x))
            {
                if(!occupied(ctx, y + d.y, x + d.x))
                {
                    if(!gen_and_call(ctx, y, x, d.y, d.x, cb, data, enforce_check))
                        return;
                }
                else
                {
                    /* occupied square */
                    if(enemy_occupied(ctx, y + d.y, x + d.x, piece->color))
                        if(!gen_and_call(ctx, y, x, d.y, d.x, cb, data, enforce_check))
                            return;
                }
            }
            ++moves;
        }
        break;
    }
    default:
        assert(false);
    }
}

struct best_data {
    int highest_score;
    int depth;
    struct move_t best_found;
};

struct worst_data {
    int lowest_score;
    int depth;
    struct move_t worst_found;
};

void print_ctx(const struct chess_ctx *ctx)
{
    for(int y = 7; y >= 0; --y)
    {
        printf("  +----+----+----+----+----+----+----+----+\n%d ", y + 1);
        for(int x = 0; x < 8; ++x)
        {
            char c = " PRNBQK"[ctx->board[y][x].type];
            if(ctx->board[y][x].color == BLACK)
                printf("| *%c ", c);
            else
                printf("|  %c ", c);
        }
        printf("|\n");
    }
    printf("  +----+----+----+----+----+----+----+----+\n    ");
    for(int i = 0; i < 8; ++i)
    {
        printf("%c    ", 'A' + i);
    }
    printf("\n");
}

const char *piece_name(enum piece type)
{
    static const char *names[] = {
        "no piece",
        "pawn",
        "rook",
        "knight",
        "bishop",
        "queen",
        "king"
    };
    return names[type];
}

void print_move(const struct chess_ctx *ctx, struct move_t move)
{
    switch(move.type)
    {
    case NOMOVE:
#ifndef UCI
        printf("No move.\n");
#endif
        return;
    case NORMAL:
    {
        /* TODO: sanity checks */
        //printf("move from %d, %d to %d, %d",
        //       move.data.normal.from.x, move.data.normal.from.y,
        //       move.data.normal.to.x, move.data.normal.to.y);
        const struct piece_t *to, *from;
        to = &ctx->board[move.data.normal.to.y][move.data.normal.to.x];
        from = &ctx->board[move.data.normal.from.y][move.data.normal.from.x];

        (void) to;
        (void) from;

        char name[3];
        name[0] = 'a' + move.data.normal.to.x;
        name[1] = '1' + move.data.normal.to.y;
        name[2] = '\0';

#ifdef UCI
        char fromname[3];
        fromname[0] = 'a' + move.data.normal.from.x;
        fromname[1] = '1' + move.data.normal.from.y;
        fromname[2] = '\0';
        printf("%s%s\n", fromname, name);
#else
        if(to->type != EMPTY)
        {
            printf("%s takes %s at %s\n", piece_name(from->type), piece_name(to->type), name);
        }
        else
        {
            printf("%s to %s\n", piece_name(from->type), name);
        }
#endif
        break;
    }
    case PROMOTION:
    {
#ifdef UCI
        char name[3];
        name[0] = 'a' + move.data.promotion.to.x;
        name[1] = '1' + move.data.promotion.to.y;
        name[2] = '\0';

        char fromname[3];
        fromname[0] = 'a' + move.data.promotion.from.x;
        fromname[1] = '1' + move.data.promotion.from.y;
        fromname[2] = '\0';
        char piecename[2];
        piecename[0] = "  rnbq"[move.data.promotion.type];
        piecename[1] = '\0';
        printf("%s%s%s\n", fromname, name, piecename);
#else
        printf("pawn promoted\n");
#endif
        break;
    }
    case CASTLE:
    {
#ifdef UCI
        const char *castle_lan[2][2] = { { "e1c1", "e1g1" },
                                         { "e8c8", "e8g8" } };
        printf("%s\n", castle_lan[move.color == WHITE ? 0 : 1][move.data.castle_style]);
#else
        printf("castles %s\n", move.data.castle_style == KINGSIDE ? "kingside" : "queenside");
#endif
        break;
    }
    default:
        assert(false);
    }
}

void execute_move(struct chess_ctx *ctx, struct move_t move)
{
    memset(&ctx->en_passant[move.color == WHITE ? 0 : 1], 0, sizeof(ctx->en_passant[0]));
    switch(move.type)
    {
    case NORMAL:
    {
        /* TODO: sanity checks */
        //printf("move from %d, %d to %d, %d",
        //       move.data.normal.from.x, move.data.normal.from.y,
        //       move.data.normal.to.x, move.data.normal.to.y);
        struct piece_t *to, *from;
        to = &ctx->board[move.data.normal.to.y][move.data.normal.to.x];
        from = &ctx->board[move.data.normal.from.y][move.data.normal.from.x];

        if(from->type == PAWN)
        {
            /* see if we've moved two squares ahead */
            if(ABS(move.data.normal.to.y - move.data.normal.from.y) == 2)
                ctx->en_passant[move.color == WHITE ? 0 : 1][move.data.normal.to.x] = true;
            else if(move.data.normal.to.x - move.data.normal.from.x != 0 && to->type == EMPTY)
            {
                /* en passant capture */
                ctx->board[move.data.normal.from.y][move.data.normal.to.x].type = EMPTY;
                ctx->board[move.data.normal.from.y][move.data.normal.to.x].color = NONE;
            }
        }
        if(from->type == KING)
        {
            ctx->king_moved[move.color == WHITE ? 0 : 1] = true;
        }
        else if(from->type == ROOK && (move.data.normal.from.x == 0 || move.data.normal.from.x == 7))
        {
            ctx->rook_moved[move.color == WHITE ? 0 : 1][move.data.normal.from.x == 0 ? 0 : 1] = true;
        }

        *to = *from;
        *from = (struct piece_t){ EMPTY, NONE };
        break;
    }
    case PROMOTION:
    {
        struct piece_t *to, *from;
        to = &ctx->board[move.data.promotion.to.y][move.data.promotion.to.x];
        from = &ctx->board[move.data.promotion.from.y][move.data.promotion.from.x];
        *from = (struct piece_t) { EMPTY, NONE };
        *to = (struct piece_t) { move.data.promotion.type, move.color };
        break;
    }
    case CASTLE:
    {
        int y = move.color == BLACK ? 7 : 0;
        int dx = move.data.castle_style == KINGSIDE ? 2 : -2;
        ctx->board[y][4].type = EMPTY;
        ctx->board[y][4].color = NONE;
        ctx->board[y][4+dx].type = KING;
        ctx->board[y][4+dx].color = move.color;
        int rx = move.data.castle_style == QUEENSIDE ? 0 : 7;
        ctx->board[y][rx].type = EMPTY;
        ctx->board[y][rx].color = NONE;
        ctx->board[y][4+dx/2].type = ROOK;
        ctx->board[y][4+dx/2].color = move.color;
        ctx->king_moved[move.color == WHITE ? 0 : 1] = true;
        break;
    }
    case NOMOVE:
        return;
    default:
        assert(false);
    }
    ctx->to_move = inv_player(ctx->to_move);
    //print_ctx(ctx);
}

struct legal_data {
    struct move_t move;
    bool legal;
};

bool legal_cb(void *data, const struct chess_ctx *ctx, struct move_t move)
{
    (void) ctx;
    struct legal_data *info = data;
    switch(info->move.type)
    {
    case PROMOTION:
    case CASTLE:
    {
        if(move.type == info->move.type &&
           move.type == CASTLE ? move.data.castle_style == info->move.data.castle_style : true)
        {
            info->legal = true;
            return false;
        }
        break;
    }
    case NORMAL:
    {
        struct coordinates *to = &move.data.normal.to;
        struct coordinates *comp = &info->move.data.normal.to;
        if(to->x == comp->x && to->y == comp->y)
        {
            info->legal = true;
            return false;
        }
        break;
    }
    default:
        assert(false);
    }
    return true;
}

bool can_castle(const struct chess_ctx *ctx, int color, int style)
{
    int k_idx = color == WHITE ? 0 : 1;
    if(ctx->king_moved[k_idx])
        return false;

    int r_idx = style == QUEENSIDE ? 0 : 1;
    if(ctx->rook_moved[k_idx][r_idx])
        return false;

    int start = (style == QUEENSIDE ? 1 : 5);
    int end = (style == QUEENSIDE ? 3 : 6);

    int y = color == WHITE ? 0 : 7;

    int rx = style == QUEENSIDE ? 0 : 7;
    if(ctx->board[y][rx].type != ROOK ||
       ctx->board[y][rx].color != color)
        return false;

    bool clear = true;
    for(int i = start; i <= end; ++i)
        if(ctx->board[y][i].type != EMPTY)
            clear = false;

    if(clear)
    {
        int dx = style == QUEENSIDE ? -1 : 1;
        if(!square_threatened(ctx, y, 4 + dx, color) &&
           !square_threatened(ctx, y, 4 + 2*dx, color))
            if(!king_in_check(ctx, color, NULL))
                return true;
    }
    return false;
}

bool legal_move(const struct chess_ctx *ctx, struct move_t move)
{
    switch(move.type)
    {
    case NORMAL:
    {
        struct coordinates from = move.data.normal.from;

        if(move.color != ctx->board[from.y][from.x].color) /* moving a different piece */
            return false;

        struct legal_data data;
        data.legal = false;
        data.move = move;
        for_each_move(ctx, move.data.normal.from.y, move.data.normal.from.x, legal_cb, &data, true, true);
        return data.legal;
    }
    case CASTLE:
    {
        return can_castle(ctx, move.color, move.data.castle_style);
    }
    default:
        return true;
    }
}

struct chess_ctx new_game(void)
{
    struct chess_ctx ret;
    memset(&ret.board, 0, sizeof(ret.board));
    for(int i = 0; i < 8; ++i)
    {
        ret.board[1][i].color = WHITE;
        ret.board[1][i].type = PAWN;
        ret.board[6][i].color = BLACK;
        ret.board[6][i].type = PAWN;
    }

    int types[] = { ROOK, KNIGHT, BISHOP };
    for(int i = 0; i < 3; ++i)
    {
        ret.board[0][i].type = types[i];
        ret.board[0][i].color = WHITE;
        ret.board[0][7 - i].type = types[i];
        ret.board[0][7 - i].color = WHITE;

        ret.board[7][i].type = types[i];
        ret.board[7][i].color = BLACK;
        ret.board[7][7 - i].type = types[i];
        ret.board[7][7 - i].color = BLACK;
    }

    ret.board[0][3].color = WHITE;
    ret.board[0][3].type = QUEEN;
    ret.board[0][4].color = WHITE;
    ret.board[0][4].type = KING;

    ret.board[7][3].color = BLACK;
    ret.board[7][3].type = QUEEN;
    ret.board[7][4].color = BLACK;
    ret.board[7][4].type = KING;
    ret.to_move = WHITE;

    ret.king_moved[0] = false;
    ret.king_moved[1] = false;
    ret.rook_moved[0][0] = false;
    ret.rook_moved[0][1] = false;
    ret.rook_moved[1][0] = false;
    ret.rook_moved[1][1] = false;

    //return ret;
    return ctx_from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", NULL);
}

struct move_t move_from_str(const struct chess_ctx *ctx, const char **line, int color)
{
    struct move_t ret;

    int x = (*line)[0] - 'a';
    int y = (*line)[1] - '1';
    int dx = (*line)[2] - 'a' - x;
    int dy = (*line)[3] - '1' - y;
    if(valid_coords(y, x) && valid_coords(y + dy, x + dx) && (dx || dy))
        ret = construct_move(color, y, x, dy, dx);
    char piece = (*line)[4];
    *line += 5;
    if(piece && piece != ' ' && piece != '\n')
    {
        /* promotion */
        (*line)++;

        ret.type = PROMOTION;
        ret.data.promotion.from = (struct coordinates) { y, x  };
        ret.data.promotion.to = (struct coordinates) { y + dy, x + dx };
        switch(toupper(piece))
        {
        case 'R':
            ret.data.promotion.type = ROOK;
            break;
        case 'N':
            ret.data.promotion.type = KNIGHT;
            break;
        case 'B':
            ret.data.promotion.type = BISHOP;
            break;
        case 'Q':
            ret.data.promotion.type = QUEEN;
            break;
        default:
            ret.type = NOMOVE;
            break;
        }
    }

    if(valid_coords(y, x) && ctx->board[y][x].type == PAWN &&
       (y + dy == 0 || y + dy == 7) && ret.type != PROMOTION)
    {
        ret.type = NOMOVE; /* we don't allow pawns on the 8th rank,
                            * they must be promoted */
    }

    if(valid_coords(y, x) && x == 4 && y == (color == WHITE ? 0 : 7) &&
       ctx->board[y][x].color == color && ctx->board[y][x].type == KING &&
       ABS(dx) == 2 && dy == 0)
    {
        /* castle */
        ret.type = CASTLE;
        ret.data.castle_style = dx > 0 ? KINGSIDE : QUEENSIDE;
    }
    return ret;
}

struct chess_ctx ctx_from_fen(const char *fen, int *len)
{
    /* no validity checking */
    char *save = NULL;
    char *str = strdup(fen);
    char *old = str;
    struct chess_ctx ret, *ctx = &ret;
    for(int i = 0; i < 8; ++i)
    {
        char *row = strtok_r(str, "/ ", &save);
        str = NULL;
        if(!row)
        {
            goto invalid;
        }
        int y = 7 - i, x = 0;
        while(*row && x < 8)
        {
            char piece = *row++;
            if(!piece)
                break;
            if(isdigit(piece))
            {
                int n = piece - '0';
                if(x + n > 8)
                {
                    goto invalid;
                }
                while(n--)
                {
                    ctx->board[y][x].type = EMPTY;
                    ctx->board[y][x].color = NONE;
                    x++;
                }
            }
            else if(isalpha(piece))
            {
                ctx->board[y][x].color = isupper(piece) ? WHITE : BLACK;
                piece = tolower(piece);
                switch(piece)
                {
                case 'p':
                    ctx->board[y][x].type = PAWN;
                    break;
                case 'r':
                    ctx->board[y][x].type = ROOK;
                    break;
                case 'n':
                    ctx->board[y][x].type = KNIGHT;
                    break;
                case 'b':
                    ctx->board[y][x].type = BISHOP;
                    break;
                case 'q':
                    ctx->board[y][x].type = QUEEN;
                    break;
                case 'k':
                    ctx->board[y][x].type = KING;
                    break;
                default:
                    goto invalid;
                }
                ++x;
            }
        }
    }
    char *tok = strtok_r(NULL, " ", &save);
    switch(tolower(tok[0]))
    {
    case 'w':
        ctx->to_move = WHITE;
        break;
    case 'b':
        ctx->to_move = BLACK;
        break;
    default:
        printf("wrong to_move\n");
        goto invalid;
    }

    /* castling */
    tok = strtok_r(NULL, " ", &save);
    while(*tok)
    {
        int idx = isupper(*tok) ? 0 : 1;
        switch(*tok)
        {
        case 'K':
        case 'k':
            ctx->king_moved[idx] = false;
            ctx->rook_moved[idx][1] = false;
            break;
        case 'Q':
        case 'q':
            ctx->king_moved[idx] = false;
            ctx->rook_moved[idx][0] = false;
            break;
        case '-':
            break;
        default:
            printf("bad castling info\n");
            goto invalid;
        }
        tok++;
    }

    memset(&ctx->en_passant, 0, sizeof(ctx->en_passant));
    tok = strtok_r(NULL, " ", &save);
    switch(tolower(tok[0]))
    {
    case '-':
        break;
    default:
    {
        unsigned int x = tolower(tok[0]) - 'a';
        unsigned int y = tok[1] - '1';
        if(x > 7 || y > 7)
        {
            printf("wrong en passant target (%u, %u, %s)\n", y, x, tok);
            goto invalid;
        }
        ctx->en_passant[y == 2 ? 0 : 1][x] = true;
        break;
    }
    }

    /* halfmove clock and fullmove number (both ignored) */
    tok = strtok_r(NULL, " ", &save);
    tok = strtok_r(NULL, " ", &save);

    /* get address of rest of string */
    tok = strtok_r(NULL, "", &save);
    if(len)
    {
        if(tok)
            *len = tok - old;
        else
            *len = strlen(fen);
    }

    free(old);
    return ret;
invalid:
    free(old);
    assert(false);
}

void parse_moves(struct chess_ctx *ctx, const char *line, int len)
{
    while(len > 3)
    {
        const char *before = line;
        struct move_t move = move_from_str(ctx, &line, ctx->to_move);
        execute_move(ctx, move);
        len -= line - before;
    }
}

struct chess_ctx get_uci_ctx(int *wtime, int *btime, int *movetime)
{
    struct chess_ctx ctx = new_game();
    while(1)
    {
        char *ptr = NULL;
        size_t sz = 0;
        ssize_t len = getline(&ptr, &sz, stdin);
        char *line = ptr;

        if(!line || !strlen(line))
        {
            free(line);
            continue;
        }

        //printf("received line: (%d, %d), \"%s\"\n", line, line[0], line);

        if(!strncasecmp(line, "uci", 3))
        {
            printf("id name XenonChess\n");
            printf("id author Franklin Wei\n");
            printf("uciok\n");
            fflush(stdout);
        }
        else if(!strncasecmp(line, "isready", 7))
        {
            printf("readyok\n");
            fflush(stdout);
        }
        else if(!strncasecmp(line, "position startpos moves ", 24))
        {
            printf("awaiting move string\n");
            ctx = new_game();

            line += 24;
            len -= 24;
            parse_moves(&ctx, line, len);
            print_ctx(&ctx);
        }
        else if(!strncasecmp(line, "go", 2))
        {
            char *save;
            char *tok;
            do {
                tok = strtok_r(line, " ", &save);
                line = NULL;
                if(!tok)
                    break;
                if(!strcmp(tok, "wtime"))
                {
                    tok = strtok_r(line, " ", &save);
                    if(tok && wtime)
                        *wtime = atoi(tok);
                }
                else if(!strcmp(tok, "btime"))
                {
                    tok = strtok_r(line, " ", &save);
                    if(tok && btime)
                        *btime = atoi(tok);
                }
                else if(!strcmp(tok, "movetime"))
                {
                    tok = strtok_r(line, " ", &save);
                    if(tok && movetime)
                        *movetime = atoi(tok);
                }
            } while(tok);
            //printf("wtime = %d, btime = %d\n", *wtime, *btime);

            free(ptr);
            return ctx;
        }
        else if(!strcasecmp(line, "position startpos\n"))
        {
            //printf("info starting move \"%s\"\n", line);
            ctx = new_game();
        }
        else if(!strncasecmp(line, "position fen ", 13))
        {
            int fenlen;
            ctx = ctx_from_fen(line + 13, &fenlen);

            line += 13 + fenlen;
            len -= 13 + fenlen;
            printf("fenlen is %d\n", fenlen);
            if(len > 0)
            {
                printf("line is \"%s\"\n", line);

                parse_moves(&ctx, line, len);
            }
            print_ctx(&ctx);
        }
        else if(!strncasecmp(line, "perft", 5))
        {
            int depth;
            if(sscanf(line, "perft %d\n", &depth) != 1)
                depth = 4;
            printf("info depth %d nodes %lu\n", depth, perft(&ctx, depth - 1));
            fflush(stdout);
        }
        else if(!strncasecmp(line, "eval", 4))
        {
            printf("info value WHITE: %d, BLACK: %d\n", eval_position(&ctx, WHITE), eval_position(&ctx, BLACK));
            fflush(stdout);
        }
        free(ptr);
    }
}

struct perft_info {
    uint64_t n;
    int depth;
};

bool perft_cb(void *data, const struct chess_ctx *ctx, struct move_t move)
{
    struct perft_info *info = data;

    if(info->depth > 0)
    {
        struct chess_ctx local = *ctx;
        execute_move(&local, move);
        uint64_t child = perft(&local, info->depth - 1);
        if(info->depth == 5)
        {
            printf("move has %"PRIu64" children: ", child);
            print_move(ctx, move);
        }
        info->n += child;
    }
    else
        info->n++;

    return true;
}

uint64_t perft(const struct chess_ctx *ctx, int depth)
{
    struct perft_info info;
    info.n = 0;
    info.depth = depth;
    for(int y = 0; y < 8; ++y)
    {
        for(int x = 0; x < 8; ++x)
        {
            if(ctx->board[y][x].color == ctx->to_move)
            {
                /* recurse */
                for_each_move(ctx, y, x, perft_cb, &info, true, true);
            }
        }
    }
    return info.n;
}

uint64_t pondered;
int moveno;

struct move_t get_move(const struct chess_ctx *ctx, enum player color)
{
    struct move_t ret;
again:
    ret.type = NOMOVE;

    char *ptr = NULL;
    size_t sz = 0;
    ssize_t len = getline(&ptr, &sz, stdin);
    char *line = ptr;

    if(!strncasecmp(line, "0-0-0", 5) || !strncasecmp(line, "O-O-O", 5))
    {
        ret.color = color;
        ret.type = CASTLE;
        ret.data.castle_style = QUEENSIDE;
        goto done;
    }
    else if(!strncasecmp(line, "0-0", 3) || !strncasecmp(line, "O-O", 3))
    {
        ret.color = color;
        ret.type = CASTLE;
        ret.data.castle_style = KINGSIDE;
        goto done;
    }
    else if(!strncasecmp(line, "uci", 3))
    {
        printf("id name XenonChess\n");
        printf("uciok\n");
        fflush(stdout);
        goto again;
    }
    else if(!strncasecmp(line, "isready", 7))
    {
        printf("readyok\n");
        fflush(stdout);
        goto again;
    }
    else if(!strncasecmp(line, "help", 4))
    {
        moveno = 0;
        best_move_negamax(ctx, DEFAULT_DEPTH, -999999, 999999, color, &ret, DEFAULT_DEPTH, -1);
        goto done;
    }
    else if(!strncasecmp(line, "perft", 5))
    {
        int depth;
        if(sscanf(line, "perft %d\n", &depth) != 1)
            depth = 4;
        printf("info depth %d nodes %lu\n", depth, perft(ctx, depth - 1));
        fflush(stdout);
        goto again;
    }
    else if(!strncasecmp(line, "eval", 4))
    {
        printf("info value WHITE: %d, BLACK: %d\n", eval_position(ctx, WHITE), eval_position(ctx, BLACK));
        fflush(stdout);
        goto again;
    }

    if(len < 5)
    {
        goto done;
    }

    ret = move_from_str(ctx, (const char**)&line, color);

done:
    free(ptr);

    return ret;
}

struct negamax_info {
    int best;
    int depth;
    int a, b;
    int full_depth;
    int stop_time;
    struct move_t move;
};

bool negamax_cb(void *data, const struct chess_ctx *ctx, struct move_t move)
{
    struct negamax_info *info = data;
    struct chess_ctx local = *ctx;

    ++pondered;

    int king_penalty = 0;
    if(move.type == NORMAL && ctx->board[move.data.normal.from.x][move.data.normal.from.y].type == KING)
        king_penalty = 100;

    if(king_in_check(ctx, ctx->to_move, NULL))
    {
        if(info->full_depth < MAX_DEPTH)
        {
#ifdef CHECK_EXTENSIONS
            info->depth++;
            info->full_depth++;
#endif
        }
    }

    execute_move(&local, move);
    int v = -(best_move_negamax(&local, info->depth - 1, -info->b, -info->a, local.to_move, NULL, info->full_depth, info->stop_time) + king_penalty);
    if(v > info->best || (v == info->best && rand() % 8 == 2))
    {
        info->best = v;
        info->move = move;
    }
    info->a = MAX(info->a, v);

    if(info->depth == info->full_depth)
    {
#if defined(UCI) || DEFAULT_DEPTH > 3
        printf("info currmove ");
        print_move(ctx, move);
        printf("info currmovenumber %d\n", ++moveno);
        fflush(stdout);
#endif
        //printf("current best: %d, ", info->best);
        //print_move(ctx, info->move);
        //printf("movescore: %d\n", v);
    }
#if DEFAULT_DEPTH > 5
    else if(info->depth == DEFAULT_DEPTH - 1)
    {
        printf("submove ");
        print_move(ctx, move);
        fflush(stdout);
    }
#endif

    if(info->a >= info->b)
        return false;
    return true;
}

int ms_time(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000 + t.tv_nsec / 1e6;
}

int best_move_negamax(const struct chess_ctx *ctx, int depth,
                      int a, int b, int color,
                      struct move_t *best, int full_depth, int stop_time)
{
    struct negamax_info info;
    info.best = -99999999;
    info.move.type = NOMOVE;
    info.depth = depth;
    info.full_depth = full_depth;
    info.a = a;
    info.b = b;
    info.stop_time = stop_time;

    if(depth > 0)
    {
        for(int y = 0; y < 8; ++y)
        {
            for(int x = 0; x < 8; ++x)
            {
                if(stop_time > 0 && ms_time() > stop_time)
                {
                    /* abort! */
                    if(best)
                        best->type = NOMOVE;
                    printf("aborting depth %d search due to time\n", info.full_depth);
                    return -99999999;
                }
                if(ctx->board[y][x].color == ctx->to_move)
                {
                    /* recurse */
                    for_each_move(ctx, y, x, negamax_cb, &info, true, true);
                }
            }
        }
        if(best)
            *best = info.move;
    }
    if(!depth || info.move.type == NOMOVE) /* terminal node */
        return eval_position(ctx, color);

    return info.best;
}

struct move_t best_move(const struct chess_ctx *ctx, int stop_time)
{
    struct move_t best;
    best.type = NOMOVE;
    if(stop_time < 0)
    {
        /* no time limit, default depth */
        best_move_negamax(ctx, DEFAULT_DEPTH, -9999999, 9999999, ctx->to_move, &best, DEFAULT_DEPTH, stop_time);
        return best;
    }
    for(int i = 1; i < MAX_DEPTH; ++i)
    {
        struct move_t old = best;
        best_move_negamax(ctx, i, -9999999, 9999999, ctx->to_move, &best, i, old.type == NOMOVE ? -1 : stop_time);
        if(ms_time() > stop_time)
        {
            if(old.type != NOMOVE)
            {
                printf("returning old result due to time\n");
                return old;
            }
            else
            {
                printf("finishing search...\n");
                /* finish the search, ignoring the time */
                best_move_negamax(ctx, i, -9999999, 9999999, ctx->to_move, &best, i, -1);
                return best;
            }
        }
        printf("after depth %d search, best move: ", i);
        print_move(ctx, best);
    }
    return best;
}

float calculate_phase(const struct chess_ctx *ctx)
{
    int mat = count_material(ctx, WHITE) + count_material(ctx, BLACK);
    static int start_material = -1;
    if(start_material < 0)
    {
        struct chess_ctx new = new_game();
        start_material = count_material(&new, WHITE) * 2;
    }
    int end_material = piece_values[KING] * 2;
    return (float)(mat - start_material) / (float)(end_material - start_material);
}

#define INTERPOLATE(a, b, x) ((a) + ((b) - (a)) * (x))

void init_pst(const struct chess_ctx *ctx)
{
    memset(location_bonuses, 0, sizeof(location_bonuses));
    float phase = calculate_phase(ctx);
    printf("game phase is %f\n", phase);
    for(int i = 0; i < 6; ++i)
        for(int y = 0; y < 8; ++y)
            for(int x = 0; x < 8; ++x)
                location_bonuses[i][y][x] = INTERPOLATE(location_bonuses_early[i][y][x],
                                                        location_bonuses_endgame[i][y][x],
                                                        phase);
}

void print_status(const struct chess_ctx *ctx)
{
    (void) ctx;
#ifndef UCI
    if(king_in_checkmate(ctx, WHITE))
    {
        printf("White is in checkmate\n");
        exit(0);
    }
    else if(king_in_check(ctx, WHITE, NULL))
        printf("White is in check\n");

    if(king_in_checkmate(ctx, BLACK))
    {
        printf("Black is in checkmate\n");
        exit(0);
    }
    else if(king_in_check(ctx, BLACK, NULL))
        printf("Black is in check\n");
#endif
}

int main()
{
    printf("XenonChess\n");
    unsigned int seed;
    int fd = open("/dev/urandom", O_RDONLY);
    read(fd, &seed, sizeof seed);
    srand(seed);

#ifndef UCI
    struct chess_ctx ctx = new_game();
    print_ctx(&ctx);
#endif

    for(;;)
    {
#ifndef AUTOMATCH
#ifndef UCI
        struct move_t player = get_move(&ctx, ctx.to_move);
        if(player.type == NOMOVE)
        {
            printf("Illegal\n");
            continue;
        }

        if(!legal_move(&ctx, player))
        {
            printf("Illegal\n");
            continue;
        }

        execute_move(&ctx, player);

        print_ctx(&ctx);
        print_status(&ctx);
#else
        int wtime = -1, btime = -1, movetime = -1;
        struct chess_ctx ctx = get_uci_ctx(&wtime, &btime, &movetime);
#endif
#endif
        int stop_time;
        int think_time = movetime > 0 ? movetime : (ctx.to_move == WHITE ? wtime : btime) / 40;
        if(think_time <= 0)
            stop_time = -1;
        else
            stop_time = ms_time() + think_time;

        printf("info Thinking...\n");
        struct move_t best;
        pondered = 0;
        moveno = 0;
        clock_t start = clock();

        init_pst(&ctx);

        best = best_move(&ctx, stop_time);
        //best_move_negamax(&ctx, DEFAULT_DEPTH, -9999999, 9999999, ctx.to_move, &best, DEFAULT_DEPTH, stop_time);
        clock_t end = clock();
        float time = (float)(end - start) / CLOCKS_PER_SEC;
        printf("bestmove ");
        print_move(&ctx, best);
        fflush(stdout);

        execute_move(&ctx, best);
#ifndef UCI
        print_ctx(&ctx);
        print_status(&ctx);
#endif

        if(time)
        {
            printf("info pondered %"PRIu64" moves in %.2f seconds (%.1f/sec)\n", pondered,
                   time, pondered / time);
        }

        if(best.type == NOMOVE)
        {
            printf("info Stalemate\n");
            return 0;
        }
    }
}
