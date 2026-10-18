/* Savestates of the port (jj_state.cpp) */
#ifndef JJ_STATE_H
#define JJ_STATE_H

class Game;

void gnw_game_created(Game *game);
void gnw_game_destroyed(Game *game);
/* a savestate is waiting to be played (the game unwinds to oj_gnw_main()) */
bool gnw_resume_pending();
/* play it: returns at the end of that game */
int gnw_resume_play();

#endif
