#include "sokoban_platform.h"
#include "wasm4.h"

#include <stdint.h>

static uint8_t prev_gamepad = 0;
struct game_input game_input;

void update() {
	uint8_t gamepad = *GAMEPAD1;
	uint8_t pressed_this_frame = gamepad & (gamepad ^ prev_gamepad);

	game_input.move_left = (pressed_this_frame & BUTTON_LEFT) ? 1 : 0;
	game_input.move_right = (pressed_this_frame & BUTTON_RIGHT) ? 1 : 0;
	game_input.move_up = (pressed_this_frame & BUTTON_UP) ? 1 : 0;
	game_input.move_down = (pressed_this_frame & BUTTON_DOWN) ? 1 : 0;
	game_input.action_x = (pressed_this_frame & BUTTON_1) ? 1 : 0;
	game_input.action_z = (pressed_this_frame & BUTTON_2) ? 1 : 0;

	game_update_and_render(&game_input);

	prev_gamepad = gamepad;
}

