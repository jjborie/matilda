/*
Functions for board scoring that take komi and dynamic komi into consideration.

Remember that in Matilda, scores and komi are always doubled to become integer.
*/

#include "matilda.h"

#include <string.h>
#include <stdio.h>
#include <omp.h>

#include "board.h"
#include "cfg_board.h"
#include "mcts.h"
#include "playout.h"
#include "scoring.h"
#include "state_changes.h"
#include "timem.h"
#include "tactical.h"
#include "alloc.h"


d16 komi_offset = 0;
extern d16 komi;

/*
Produces a textual representation of a Go match score., ex: B+3.5, 0
*/
void score_to_string(
    char * dst,
    d16 score
){
    if(score == 0)
        snprintf(dst, MAX_PAGE_SIZ, "0");
    else
        if((score & 1) == 1)
        {
            if(score > 0)
                snprintf(dst, MAX_PAGE_SIZ, "B+%d.5", score / 2);
            else
                snprintf(dst, MAX_PAGE_SIZ, "W+%d.5", (-score) / 2);
        }
        else
        {
            if(score > 0)
                snprintf(dst, MAX_PAGE_SIZ, "B+%d", score / 2);
            else
                snprintf(dst, MAX_PAGE_SIZ, "W+%d", (-score) / 2);
        }
}

/*
Produces a textual representation of a komidashi value.
*/
void komi_to_string(
    char * dst,
    d16 komi
){
    if(komi == 0)
        snprintf(dst, MAX_PAGE_SIZ, "0");
    else
        if((komi & 1) == 1)
        {
            if(komi > 0)
                snprintf(dst, MAX_PAGE_SIZ, "%d.5", komi / 2);
            else
                snprintf(dst, MAX_PAGE_SIZ, "-%d.5", (-komi) / 2);
        }
        else
        {
            if(komi > 0)
                snprintf(dst, MAX_PAGE_SIZ, "%d", komi / 2);
            else
                snprintf(dst, MAX_PAGE_SIZ, "-%d", (-komi) / 2);
        }
}

/*
Scoring by counting stones on the board only.
RETURNS positive score for a black win; negative for a white win; 0 for a draw
*/
d16 score_stones_only(
    const u8 p[TOTAL_BOARD_SIZ]
){
    d16 r = 0;
    for(move m = 0; m < TOTAL_BOARD_SIZ; ++m)
        switch(p[m])
        {
            case BLACK_STONE:
                r += 2;
                break;
            case WHITE_STONE:
                r -= 2;
                break;
        }

    return r - komi - komi_offset;
}


/*
Scoring by counting stones and eyes on the board only.
RETURNS positive score for a black win; negative for a white win; 0 for a draw
*/
d16 score_stones_and_eyes2(
    const cfg_board * cb
){
    bool _ignored;
    d16 r = 0;
    for(move m = 0; m < TOTAL_BOARD_SIZ; ++m)
        switch(cb->p[m])
    {
            case BLACK_STONE:
                r += 2;
                break;
            case WHITE_STONE:
                r -= 2;
                break;
            case EMPTY:
                if(is_4pt_eye(cb, m, true, &_ignored))
                {
                    r += 8;
                    ++m;
                    break;
                }
                if(is_4pt_eye(cb, m, false, &_ignored))
                {
                    r -= 8;
                    ++m;
                    break;
                }
                if(is_2pt_eye(cb, m, true, &_ignored))
                {
                    r += 4;
                    break;
                }
                if(is_2pt_eye(cb, m, false, &_ignored))
                {
                    r -= 4;
                    break;
                }
                if(is_eye(cb, m, true))
                {
                    r += 2;
                    break;
                }
                if(is_eye(cb, m, false))
                {
                    r -= 2;
                    break;
                }
        }

    return r - komi - komi_offset;
}

/*
Scoring by counting stones and eyes on the board only.
RETURNS positive score for a black win; negative for a white win; 0 for a draw
*/
d16 score_stones_and_eyes(
    const board * b
){
    cfg_board cb;
    cfg_from_board(&cb, b);
    d16 ret = score_stones_and_eyes2(&cb);
    cfg_board_free(&cb);
    return ret;
}


static void _search(
    const u8 p[TOTAL_BOARD_SIZ],
    move m,
    bool explored[TOTAL_BOARD_SIZ],
    bool * black,
    bool * white
){
    u8 x;
    u8 y;
    move_to_coord(m, &x, &y);

    if(x > 0)
    {
        if(p[m + LEFT] == BLACK_STONE)
            *black |= true;
        else
            if(p[m + LEFT] == WHITE_STONE)
                *white |= true;
            else
                if(explored[m + LEFT] == false)
                {
                    explored[m + LEFT] = true;
                    _search(p, m + LEFT, explored, black, white);
                }
    }

    if(x < BOARD_SIZ - 1)
    {
        if(p[m + RIGHT] == BLACK_STONE)
            *black |= true;
        else
            if(p[m + RIGHT] == WHITE_STONE)
                *white |= true;
            else
                if(explored[m + RIGHT] == false)
                {
                    explored[m + RIGHT] = true;
                    _search(p, m + RIGHT, explored, black, white);
                }
    }

    if(y > 0)
    {
        if(p[m + TOP] == BLACK_STONE)
            *black |= true;
        else
            if(p[m + TOP] == WHITE_STONE)
                *white |= true;
            else
                if(explored[m + TOP] == false)
                {
                    explored[m + TOP] = true;
                    _search(p, m + TOP, explored, black, white);
                }
    }

    if(y < BOARD_SIZ - 1)
    {
        if(p[m + BOTTOM] == BLACK_STONE)
            *black |= true;
        else
            if(p[m + BOTTOM] == WHITE_STONE)
                *white |= true;
            else
                if(explored[m + BOTTOM] == false)
                {
                    explored[m + BOTTOM] = true;
                    _search(p, m + BOTTOM, explored, black, white);
                }
    }
}

static void _apply(
    u8 p[TOTAL_BOARD_SIZ],
    move m,
    u8 val
){
    u8 x;
    u8 y;
    move_to_coord(m, &x, &y);

    if(x > 0 && p[m + LEFT] == EMPTY)
    {
        p[m + LEFT] = val;
        _apply(p, m + LEFT, val);
    }

    if(x < BOARD_SIZ - 1 && p[m + RIGHT] == EMPTY)
    {
        p[m + RIGHT] = val;
        _apply(p, m + RIGHT, val);
    }

    if(y > 0 && p[m + TOP] == EMPTY)
    {
        p[m + TOP] = val;
        _apply(p, m + TOP, val);
    }

    if(y < BOARD_SIZ - 1 && p[m + BOTTOM] == EMPTY)
    {
        p[m + BOTTOM] = val;
        _apply(p, m + BOTTOM, val);
    }
}

/*
Scoring by counting stones and surrounded area. Also known as area scoring. Does
not remove dead stones.
RETURNS positive score for a black win; negative for a white win; 0 for a draw
*/
d16 score_stones_and_area(
    const u8 p[TOTAL_BOARD_SIZ]
){
    /* explored intersections array is only used for empty intersections */
    bool explored[TOTAL_BOARD_SIZ];
    memset(explored, false, TOTAL_BOARD_SIZ * sizeof(bool));

    u8 bak[TOTAL_BOARD_SIZ];
    memcpy(bak, p, TOTAL_BOARD_SIZ);

    for(move m = 0; m < TOTAL_BOARD_SIZ; ++m)
        if(p[m] == EMPTY && !explored[m]) /* Find owner of empty intersection */
        {
            bool found_black = false;
            bool found_white = false;
            explored[m] = true;
            _search(p, m, explored, &found_black, &found_white);
            if(found_black != found_white) /* established intersection */
            {
                if(found_black)
                {
                    bak[m] = BLACK_STONE;
                    _apply(bak, m, BLACK_STONE);
                }
                else
                {
                    bak[m] = WHITE_STONE;
                    _apply(bak, m, WHITE_STONE);
                }
            }
        }

    d16 r = 0;
    for(move m = 0; m < TOTAL_BOARD_SIZ; ++m)
        if(bak[m] == BLACK_STONE)
            r += 2;
        else
            if(bak[m] == WHITE_STONE)
                r -= 2;

    return r - komi - komi_offset;
}

/*
Scoring by estimating the final status of each intersection by running MCTS.
This method is more accurate and fit for any part of the game, but much slower.
Each intersection is awarded the player that has a majority of stones there in
the end. The simulations ignore superkos. After simulating the final result,
area scoring is used.
RETURNS positive score for a black win; negative for a white win; 0 for a draw
*/
d16 score_estimate(
    const board * b,
    bool is_black
){
    u8 e[TOTAL_BOARD_SIZ];
    estimate_final_position(e, b, is_black);

    /* Apply area scoring */
    return score_stones_and_area(e);
}

/*
Estimate the final game position from the current state. Is the most accurate
the later in the game.
*/
void estimate_final_position(
    u8 e[TOTAL_BOARD_SIZ],
    const board * b,
    bool is_black
){
    /*
    Final position estimation
    */
    u32 black_ownership[TOTAL_BOARD_SIZ];
    u32 white_ownership[TOTAL_BOARD_SIZ];
    out_board out_b;
    enable_estimate_score();
    u64 curr_time = current_time_in_millis();
    u64 stop_time = curr_time + 1000;
    mcts_start(&out_b, b, is_black, stop_time, stop_time);
    disable_estimate_score(black_ownership, white_ownership);

    /*
    Making most likely final position
    */
    memset(e, EMPTY, TOTAL_BOARD_SIZ);

    double ownership;
    for(move m = 0; m < TOTAL_BOARD_SIZ; ++m)
    {
        u32 sum = black_ownership[m] + white_ownership[m];
        if(sum == 0)
            continue;

        ownership = ((double)black_ownership[m]) / sum;
        if(ownership > 0.8)
            e[m] = BLACK_STONE;
        else
            if(ownership < 0.2)
                e[m] = WHITE_STONE;
    }
}

/*
Attempts to identify and remove dead groups.
*/
void remove_dead_stones(
    board * b
){
    board bak;
    bool save[TOTAL_BOARD_SIZ];
    memset(save, false, TOTAL_BOARD_SIZ * sizeof(bool));
    bool placed;

    /* unconditionally alive white stones */
    memcpy(&bak, b, sizeof(board));
    placed = true;
    while(placed)
    {
        placed = false;
        for(move m = 0; m < TOTAL_BOARD_SIZ; ++m)
            if(attempt_play_slow(&bak, m, true))
                placed = true;
    }
    for(move m = 0; m < TOTAL_BOARD_SIZ; ++m)
        if(bak.p[m] == WHITE_STONE)
            save[m] = true;

    /* unconditionally alive black stones */
    memcpy(&bak, b, sizeof(board));
    placed = true;
    while(placed)
    {
        placed = false;
        for(move m = 0; m < TOTAL_BOARD_SIZ; ++m)
            if(attempt_play_slow(&bak, m, false))
                placed = true;
    }
    for(move m = 0; m < TOTAL_BOARD_SIZ; ++m)
        if(bak.p[m] == BLACK_STONE)
            save[m] = true;

    for(move m = 0; m < TOTAL_BOARD_SIZ; ++m)
        if(!save[m])
            b->p[m] = EMPTY;
}


