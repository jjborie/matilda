/*
Heuristic UCT-RAVE implementation.

With UCB1-TUNED, RAVE and criticality.
Playout is limited with dynamic offset depending on stone count.
Cutoff playouts are rated. Playouts are cut short with a mercy threshold (like
pachi, orego and others).
Initilizes expanded states with prior values.
Last-good-reply with forgetting (LGRF1) is also used.
A virtual loss is also added on play traversion, that is later corrected if
needed.

MCTS can be resumed on demand by a few extra simulations at a time.
It can also record the average final score, for the purpose of score estimation.
*/

#ifndef MATILDA_MCTS_H
#define MATILDA_MCTS_H

#include "matilda.h"

#include "board.h"
#include "types.h"

/*
When to resign instead of insisting on playing.
*/
#define UCT_RESIGN_PLAYOUTS 100
#define UCT_RESIGN_WINRATE 0.10

/*
When to stop the search early because it is already very overwhelmingly
positive.
*/
#define UCT_CAN_STOP_EARLY true
#define UCT_EARLY_WINRATE 0.95


#define USE_UCT_BRANCH_LIMITER true

#define MAX_UCT_DEPTH ((TOTAL_BOARD_SIZ * 2) / 3)


/*
Constant used as coefficient of UCB contribution in UCT formula.
*/
#if BOARD_SIZ < 12
/*
Tuned with CLOP in 9x9 with 1k playouts/turn vs GNU Go 3.8 lvl 1. 4285 games.
*/
#define UCB1_C 0.867924
#else
/*
Tuned with CLOP in 13x13 with 10k playouts/turn vs GNU Go 3.8 lvl 1. 2151 games.
*/
#define UCB1_C 0.854524
#endif



/*
Instruct MCTS to take not of final positions, for final score estimation. The
results are gathered and return when calling disable_estimate_score.
*/
void enable_estimate_score();

/*
Disable score estimation and return the number of times each position belonged
to each player color.
*/
void disable_estimate_score(
    u32 black_occupied[TOTAL_BOARD_SIZ],
    u32 white_occupied[TOTAL_BOARD_SIZ]
);

/*
Performs a MCTS in at least the available time.

The search may end early if the estimated win rate is very one sided, in which
case the play selected is a pass. The search is also interrupted if memory runs
out.
RETURNS true if a play or pass is suggested instead of resigning
*/
bool mcts_start(
    out_board * out_b,
    const board * b,
    bool is_black,
    u64 stop_time,
    u64 early_stop_time
);

/*
Reset whether MCTS can run in the background after a previous attempt may have
run out of memory.
*/
void reset_mcts_can_resume();

/*
Continue a previous MCTS.
*/
void mcts_resume(
    const board * b,
    bool is_black
);

#endif


