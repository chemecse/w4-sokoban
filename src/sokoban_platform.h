#ifndef SOKOBAN_PLATFORM_H
#define SOKOBAN_PLATFORM_H

#include <stdint.h>

struct game_input {
	int32_t move_up;
	int32_t move_down;
	int32_t move_left;
	int32_t move_right;
	int32_t action_x;
	int32_t action_z;
	int32_t reset;
	int32_t cycle_level;
};

void game_update_and_render(struct game_input *input);

#endif

