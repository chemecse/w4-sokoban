#include "sokoban_platform.h"
#include "sokoban_levels.h"
#include "wasm4.h"

#include <assert.h>

/*
 * TODOs:
 * - Add screen transitions
 * - Distribute on itch
 */

#define XY(level, x, y) level->map[y][x]

const uint16_t color_one = 1;
const uint16_t color_two = 2;
const uint16_t color_three = 3;
const uint16_t color_four = 4;

enum game_screen {
	GAME_SCREEN_TITLE,
	GAME_SCREEN_PLAY,
	GAME_SCREEN_CREDITS,
};

static struct game_state {
	int32_t is_initialized;
	uint32_t frame_count;
	enum game_screen current_screen;
	int32_t current_level_idx;
	int32_t player_x;
	int32_t player_y;
} state;

static void draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint16_t color) {
	*DRAW_COLORS = color;
	/* vertically flip the coordinate system so that (0, 0) is the lower-left corner */
	rect(x, SCREEN_SIZE - y - width, width, height);
}

static void draw_bordered_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint16_t color, uint16_t border_color) {
	int32_t border_size = 2;
	draw_rect(x, y, width, height, border_color);
	draw_rect(x + border_size, y + border_size, width - border_size * 2, height - border_size * 2, color);
}

static void draw_checkerboard() {
	for (int32_t i = 0; i < SCREEN_SIZE; i += 10) {
		int32_t is_row_even = (i / 10) % 2 == 0;
		for (int32_t j = 0; j < SCREEN_SIZE; j += 10) {
			int32_t is_col_even = (j / 10) % 2 == 0;
			if ((is_row_even && is_col_even) || (!is_row_even && !is_col_even)) {
				draw_rect(j, i, 10, 10, color_two);
			}
		}
	}
}

static void shadow_text(const char *value, int32_t x, int32_t y, int32_t blink) {
	if (!blink) {
		*DRAW_COLORS = 3;
		text(value, x - 1, y - 1);
	}
	*DRAW_COLORS = 4;
	text(value, x, y);
}

GAME_UPDATE_AND_RENDER(game_update_and_render) {
	if (!state.is_initialized) {
		state.is_initialized = 1;
		state.frame_count = 0;
		state.current_screen = GAME_SCREEN_TITLE;
		state.current_level_idx = 0;
	}

	const int32_t tile_dim = 10;

	if (state.current_screen == GAME_SCREEN_TITLE) {
		if (input->action_x) {
			state.current_screen = GAME_SCREEN_PLAY;
			state.current_level_idx = 0;
		} else {
			shadow_text("Sokoban", 53, 20, 0);

			shadow_text("Solve", 61, 65, 0);
			shadow_text("puzzles.", 52, 75, 0);

			int32_t patrol_width = 9;
			int32_t patrol_height = 5;
			int32_t patrol_inverse_speed = 5;

			static int32_t x = 0;
			static int32_t y = 0;
			if (state.frame_count % patrol_inverse_speed == 0) {
				if (y == 0 && x != patrol_width) {
					x += 1;
				} else if (y == patrol_height && x != 0) {
					x -= 1;
				} else if (x == 0) {
					y -= 1;
				} else if (x == patrol_width) {
					y += 1;
				}
			}
			
			int32_t offset_x = 20;
			int32_t offset_y = 45;
			draw_bordered_rect(x * tile_dim + offset_x + tile_dim, SCREEN_SIZE - (y * tile_dim + offset_y + tile_dim), tile_dim, tile_dim, color_two, color_three);
			
			static int32_t blink = 0;
			if (state.frame_count % 10 == 0) blink = !blink;
			shadow_text("Press X to start!", 14, 120, blink);
			shadow_text("By chemecse", 38, 140, 0);
		}
	}

	if (state.current_screen == GAME_SCREEN_PLAY) {
		struct level *level = &levels[state.current_level_idx];

		if (level->state == LEVEL_START) {
			state.player_x = level->start_x;
			state.player_y = level->start_y;
			
			assert(level->block_count < MAX_BLOCK_COUNT);
			for (int32_t i = 0; i < level->block_count; i++) {
				level->block_x[i] = level->block_start_x[i];
				level->block_y[i] = level->block_start_y[i];
			}
			level->state = LEVEL_IN_PROGRESS;
		}

		int32_t is_level_complete = 1;
		for (int32_t i = 0; i < level->block_count; i++) {
			int32_t is_end_block_covered = 0;
			int32_t end_x = level->block_end_x[i];
			int32_t end_y = level->block_end_y[i];
			for (int32_t j = 0; j < level->block_count; j++) {
				if (level->block_x[j] == end_x && level->block_y[j] == end_y) {
					is_end_block_covered = 1;
					break;
				}
			}
			if (!is_end_block_covered) {
				is_level_complete = 0;
				break;
			}
		}
		if (is_level_complete) {
			level->state = LEVEL_END;
		}

		if (level->state == LEVEL_IN_PROGRESS) {
			if (input->action_z) {
				level->state = LEVEL_START;
			}

			/* movement logic */
			int32_t move_x = input->move_right - input->move_left;	
			int32_t move_y = input->move_up - input->move_down;	
			int32_t future_x = state.player_x + move_x;	
			int32_t future_y = state.player_y + move_y;	

			/* clip to level */
			if (future_x >= level->width) {
				future_x = level->width - 1;
			}
			if (future_x < 0) {
				future_x = 0;
			}
			if (future_y >= level->height) {
				future_y = level->height - 1;
			}
			if (future_y < 0) {
				future_y = 0;
			}

			int32_t had_block_interaction = 0;
			for (int32_t i = 0; i < level->block_count; i++) {
				int32_t end_x = level->block_end_x[i];
				int32_t end_y = level->block_end_y[i];
		
				if (future_x == level->block_x[i] && future_y == level->block_y[i]) {
					/* handle block interaction */
					int32_t future_block_x = level->block_x[i];
					int32_t future_block_y = level->block_y[i];
					if (move_x) {
						future_block_x += move_x;
						
					} else if (move_y) {
						future_block_y += move_y;

					}
					if (XY(level, future_block_x, future_block_y) != 1) {
						int32_t has_block_on_block_collision = 0;
						for (int32_t j = 0; j < level->block_count; j++) {
							if (level->block_x[j] == future_block_x && level->block_y[j] == future_block_y) {
								has_block_on_block_collision = 1;
								break;
							}
						}
						if (!has_block_on_block_collision) {
							level->block_x[i] = future_block_x;
							level->block_y[i] = future_block_y;
							state.player_x = future_x;
							state.player_y = future_y;
						}
					}
					had_block_interaction = 1;
					break;
				}
			}
			if (!had_block_interaction && XY(level, future_x, future_y) != 1) {
				/* handle unrestricted player movement */
				state.player_x = future_x;
				state.player_y = future_y;
			}
		} else if (level->state == LEVEL_END) {
			if (input->action_x) {
				/* cycle to the next level */
				state.current_level_idx += 1;
				levels[state.current_level_idx].state = LEVEL_START;
				if (state.current_level_idx >= LEVEL_COUNT) {
					state.current_screen = GAME_SCREEN_CREDITS;
				}
			}
		}

		/* draw background */
		draw_rect(0, 0, SCREEN_SIZE, SCREEN_SIZE, color_one);

#if 0
		/* draw checkerboard */
		draw_checkerboard();
#endif

		/* draw level map */
		int32_t offset_x = (SCREEN_SIZE / 2) - (level->width * tile_dim / 2);
		int32_t offset_y = (SCREEN_SIZE / 2) - (level->height * tile_dim/ 2);

		for (int32_t i = 0; i < level->height; i++) {
			for (int32_t j = 0; j < level->width; j++) {
				int32_t is_wall = XY(level, j, i) == 1;
				draw_rect(j * tile_dim + offset_x, i * tile_dim + offset_y, tile_dim, tile_dim, is_wall ? color_four : color_one);
			}

		}

		/* draw block end spots */
		for (int32_t i = 0; i < level->block_count; i++) {
			draw_rect(level->block_end_x[i] * tile_dim + offset_x, level->block_end_y[i] * tile_dim + offset_y, tile_dim, tile_dim, color_three);
		}

		/* draw blocks */
		for (int32_t i = 0; i < level->block_count; i++) {
			draw_rect(level->block_x[i] * tile_dim + offset_x, level->block_y[i] * tile_dim + offset_y, tile_dim, tile_dim, color_two);
		}

		/* draw player */
		draw_bordered_rect(state.player_x * tile_dim + offset_x, state.player_y * tile_dim + offset_y, tile_dim, tile_dim, color_two, color_three);

		/* draw text */
		shadow_text(level->name, 53, 3, 0);
		if (level->state == LEVEL_IN_PROGRESS) {
			shadow_text("Press Z to restart", 10, SCREEN_SIZE - 10, 0);
		} else if (level->state == LEVEL_END) {
			static int32_t blink = 0;
			if (state.frame_count % 10 == 0) blink = !blink;
			shadow_text("Press X for next", 17, SCREEN_SIZE - 10, blink);
		}
	}

	if (state.current_screen == GAME_SCREEN_CREDITS) {
		if (input->action_z) {
			state.current_screen = GAME_SCREEN_TITLE;
		}

		shadow_text("Sokoban", 53, 20, 0);

		shadow_text("You finished!!!", 23, 70, 0);

		static int32_t blink = 0;
		if (state.frame_count % 10 == 0) blink = !blink;
		shadow_text("Press Z to replay!", 11, 120, blink);
		shadow_text("By chemecse", 38, 140, 0);
	}

	state.frame_count += 1;
}

