#include <stdio.h>
#include <string.h>
#include "lib/SMSlib.h"
#include "lib/PSGlib.h"
#include "data.h"
#include "actor.h"

#define SCREEN_W (256)
#define SCREEN_H (192)
#define SCROLL_H (224)

#define MAX_PLAYERS (4)
#define MAX_SPAWNERS (5)
#define MAX_ACTORS (MAX_PLAYERS + MAX_SPAWNERS * 2)
#define FOREACH_ACTOR(act) actor *act = actors; for (char idx_##act = 0; idx_##act != MAX_ACTORS; idx_##act++, act++)
	
#define ANIMATION_SPEED (3)

#define PLAYER_SPEED (3)
#define PLAYER_TOP (32)
#define PLAYER_LEFT (8)
#define PLAYER_BOTTOM (SCREEN_H - 24)

#define GROUP_RED_CAR (1)
#define GROUP_BIKE (2)

#define SCORE_DIGITS (6)

#define LEVEL_DIGITS (3)

#define RESCUE_CHARS (6)

#define LIFE_CHARS (6)

#define STATE_START (1)
#define STATE_GAMEPLAY (2)
#define STATE_GAMEOVER (3)

actor actors[MAX_ACTORS];

actor *player1 = actors;
actor *player2 = actors + 1;
actor *player3 = actors + 2;
actor *player4 = actors + 3;
actor *first_spawner = actors + MAX_PLAYERS;

int animation_delay;

struct score {
	unsigned int value;
	char dirty;
} score;

struct rescue {
	int value;
	char dirty;
} rescue;

struct life {
	int value;
	char dirty;
} life;

struct level {
	unsigned int number;
	char starting;
	char ending;

	unsigned int submarine_score;
	unsigned int fish_score;
	unsigned int diver_score;
	
	unsigned char submarine_speed;
	unsigned char fish_speed;
	unsigned char diver_speed;
	
	int boost_chance;
	char enemy_can_fire;
	char show_diver_indicator;
} level;

void add_score(unsigned int value);
void add_rescue(int value);
void add_life(int value);

void clear_actors() {
	FOREACH_ACTOR(act) {
		act->active = 0;
	}
}

void move_actors() {
	FOREACH_ACTOR(act) {
		move_actor(act);
	}
}

void draw_actors() {
	FOREACH_ACTOR(act) {
		draw_actor(act);
	}
}

void interrupt_handler() {
	PSGFrame();
	PSGSFXFrame();	
}

void load_standard_palettes() {
	SMS_loadBGPalette(sprites_palette_bin);
	SMS_loadSpritePalette(sprites_palette_bin);
	SMS_setBGPaletteColor(0, 0x2A);
	SMS_setSpritePaletteColor(0, 0);
}

void shuffle_random(char times) {
	for (; times; times--) {
		rand();
	}
}

void handle_player_input() {
	unsigned int joy = SMS_getKeysStatus();

	// Player 1
	if (joy & PORT_A_KEY_UP) {
		if (player1->y > PLAYER_TOP) player1->y -= PLAYER_SPEED;
		player1->facing_left = 0;
		shuffle_random(1);
	} else if (joy & PORT_A_KEY_DOWN) {
		if (player1->y < PLAYER_BOTTOM) player1->y += PLAYER_SPEED;
		player1->facing_left = 1;
		shuffle_random(2);
	}
	
	// Player 2
	if (joy & PORT_A_KEY_2) {
		if (player2->y > PLAYER_TOP) player2->y -= PLAYER_SPEED;
		player2->facing_left = 0;
		shuffle_random(1);
	} else if (joy & PORT_A_KEY_1) {
		if (player2->y < PLAYER_BOTTOM) player2->y += PLAYER_SPEED;
		player2->facing_left = 1;
		shuffle_random(2);
	}

	// Player 3
	if (joy & PORT_B_KEY_UP) {
		if (player3->y > PLAYER_TOP) player3->y -= PLAYER_SPEED;
		player3->facing_left = 0;
		shuffle_random(1);
	} else if (joy & PORT_B_KEY_DOWN) {
		if (player3->y < PLAYER_BOTTOM) player3->y += PLAYER_SPEED;
		player3->facing_left = 1;
		shuffle_random(2);
	}

	// Player 4
	if (joy & PORT_B_KEY_2) {
		if (player4->y > PLAYER_TOP) player4->y -= PLAYER_SPEED;
		player4->facing_left = 0;
		shuffle_random(1);
	} else if (joy & PORT_B_KEY_1) {
		if (player4->y < PLAYER_BOTTOM) player4->y += PLAYER_SPEED;
		player4->facing_left = 1;
		shuffle_random(2);
	}
}

void adjust_facing(actor *act, char facing_left) {
	static actor *_act;
	_act = act;
	
	_act->facing_left = facing_left;
	if (facing_left) {
		_act->x = SCREEN_W - _act->x;
		_act->spd_x = -_act->spd_x;
	} else {
		_act->x -= _act->pixel_w;
	}
}

void handle_spawners() {
	static actor *act, *act2;
	static char i, facing_left, thing_to_spawn, boost;
	static int y;
	
	act = first_spawner;
	for (i = 0, y = PLAYER_TOP + 10; i != MAX_SPAWNERS; i++, act += 2, y += 24) {
		act2 = act + 1;
		if (!act->active && !act2->active) {
			if (rand() & 3 > 1) {
				// Always spawn from the left
				facing_left = 0;
				thing_to_spawn = (rand() >> 4) & 1;
				boost = (rand() >> 4) % level.boost_chance ? 0 : 1;
				
				switch (thing_to_spawn) {
				case 0:
					// Spawn a red car
					init_actor(act, 0, y, 3, 1, 66, 1);
					act->spd_x = level.submarine_speed + boost;
					act->group = GROUP_RED_CAR;
					act->score = level.submarine_score;
					break;
					
				case 1:
					// Spawn a pair of bikes
					init_actor(act, 0, y, 2, 1, 128, 1);
					init_actor(act2, -64, y, 2, 1, 128, 1);
					act->spd_x = level.fish_speed + boost;
					act->group = GROUP_BIKE;
					act->score = level.fish_score;

					act2->spd_x = act->spd_x;
					act2->group = act->group;
					act2->score = act->score;
					break;
				}
				
				adjust_facing(act, facing_left);
				adjust_facing(act2, facing_left);
			}	
		}
	}
}

void draw_background() {
}

char is_touching(actor *act1, actor *act2) {
	static actor *collider1, *collider2;
	static int r1_tlx, r1_tly, r1_brx, r1_bry;
	static int r2_tlx, r2_tly, r2_bry;

	// Use global variables for speed
	collider1 = act1;
	collider2 = act2;

/*
	// Rough collision: check if their base vertical coordinates are on the same row
	if (abs(collider1->y - collider2->y) > 16) {
		return 0;
	}
	
	// Rough collision: check if their base horizontal coordinates are not too distant
	if (abs(collider1->x - collider2->x) > 24) {
		return 0;
	}
	*/
	
	// Less rough collision on the Y axis
	
	r1_tly = collider1->y + collider1->col_y;
	r1_bry = r1_tly + collider1->col_h;
	r2_tly = collider2->y + collider2->col_y;
	
	// act1 is too far above
	if (r1_bry < r2_tly) {
		return 0;
	}
	
	r2_bry = r2_tly + collider2->col_h;
	
	// act1 is too far below
	if (r1_tly > r2_bry) {
		return 0;
	}
	
	// Less rough collision on the X axis
	
	r1_tlx = collider1->x + collider1->col_x;
	r1_brx = r1_tlx + collider1->col_w;
	r2_tlx = collider2->x + collider2->col_x;
	
	// act1 is too far to the left
	if (r1_brx < r2_tlx) {
		return 0;
	}
	
	int r2_brx = r2_tlx + collider2->col_w;
	
	// act1 is too far to the left
	if (r1_tlx > r2_brx) {
		return 0;
	}
	
	return 1;
}

// Made global for performance
actor *collider;

void check_collision_against_player() {	
	if (!collider->active || !collider->group) {
		return;
	}

	if (player1->active && is_touching(collider, player1)) {
		collider->active = 0;		
		player1->active = 0;
		
		add_score(collider->score);
	}
}

void check_collisions() {
	FOREACH_ACTOR(act) {
		collider = act;
		check_collision_against_player();
	}
}

void reset_actors_and_player() {
	clear_actors();
	init_actor(player1, 64, PLAYER_BOTTOM, 2, 1, 2, 4);
	init_actor(player2, 106, PLAYER_BOTTOM, 2, 1, 2, 4);	
	init_actor(player3, 150, PLAYER_BOTTOM, 2, 1, 2, 4);	
	init_actor(player4, 192, PLAYER_BOTTOM, 2, 1, 2, 4);	
}

void set_score(unsigned int value) {
	score.value = value;
	score.dirty = 1;
}

void add_score(unsigned int value) {
	set_score(score.value + value);
}

void draw_score() {
	static char buffer[SCORE_DIGITS];
	
	memset(buffer, -1, sizeof buffer);
	
	// Last digit is always zero
	char *d = buffer + SCORE_DIGITS - 1;
	*d = 0;
	d--;
	
	// Calculate the digits
	unsigned int remaining = score.value;
	while (remaining) {
		*d = remaining % 10;		
		remaining = remaining / 10;
		d--;
	}
		
	// Draw the digits
	d = buffer;
	SMS_setNextTileatXY(((32 - SCORE_DIGITS) >> 1) + 1, 0);
	for (char i = SCORE_DIGITS; i; i--, d++) {
		SMS_setTile((*d << 1) + 237 + TILE_USE_SPRITE_PALETTE);
	}
}

void draw_score_if_needed() {
	if (score.dirty) draw_score();
}

void draw_level_number() {
	static char buffer[LEVEL_DIGITS];
	
	memset(buffer, -1, sizeof buffer);
	
	// Calculate the digits
	char *d = buffer + LEVEL_DIGITS - 1;
	unsigned int remaining = level.number;
	do {
		*d = remaining % 10;		
		remaining = remaining / 10;
		d--;
	} while (remaining);
		
	// Draw the digits
	d = buffer;
	SMS_setNextTileatXY(2, 0);
	for (char i = LEVEL_DIGITS; i; i--, d++) {
		SMS_setTile((*d << 1) + 237 + TILE_USE_SPRITE_PALETTE);
	}
}

void set_rescue(int value) {
	if (value < 0) value = 0;
	if (value > RESCUE_CHARS) value = RESCUE_CHARS;
	rescue.value = value;
	rescue.dirty = 1;	
}

void add_rescue(int value) {
	set_rescue(rescue.value + value);	
}

void draw_rescue() {
	static char blink_control;
	
	SMS_setNextTileatXY(32 - RESCUE_CHARS - 2, 1);
	
	int remaining = rescue.value;
	
	// Blink if all divers rescued.
	if (rescue.value == RESCUE_CHARS) {
		if (blink_control & 0x10) remaining = 0;
		blink_control++;
	}
	
	for (char i = RESCUE_CHARS; i; i--) {
		SMS_setTile((remaining > 0 ? 63 : 62) + TILE_USE_SPRITE_PALETTE);
		remaining --;
	}
}

void draw_rescue_if_needed() {
	if (rescue.dirty) draw_rescue();
}

void set_life(int value) {
	if (value < 0) value = 0;
	life.value = value;
	life.dirty = 1;	
}

void add_life(int value) {
	set_life(life.value + value);	
}

void draw_life() {
	SMS_setNextTileatXY(2, 1);
	
	int remaining = life.value;
	for (char i = LIFE_CHARS; i; i--) {
		SMS_setTile((remaining > 0 ? 61 : 60) + TILE_USE_SPRITE_PALETTE);
		remaining --;
	}
}

void draw_life_if_needed() {
	if (rescue.dirty) draw_life();
}

void handle_oxygen() {
	if (level.starting) {			
		level.starting = 0;
	}
}

void initialize_level() {
	level.starting = 1;
	level.ending = 0;
	
	clear_actors();
	set_rescue(0);
	
	level.fish_score = 1 + level.number / 3;
	level.submarine_score = level.fish_score << 1;
	level.diver_score = level.fish_score + level.submarine_score;
	
	level.fish_speed = 1 + level.number / 3;
	level.submarine_speed = 1 + level.number / 5;
	level.diver_speed = 1 + level.number / 6;
	
	if (level.fish_speed > PLAYER_SPEED) level.fish_speed = PLAYER_SPEED;
	if (level.submarine_speed > PLAYER_SPEED) level.submarine_speed = PLAYER_SPEED;
	if (level.diver_speed > PLAYER_SPEED) level.diver_speed = PLAYER_SPEED;
	
	level.enemy_can_fire = 1;
	level.show_diver_indicator = level.number < 2;
	
	level.boost_chance = 14 - level.number * 2 / 3;
	if (level.boost_chance < 2) level.boost_chance = 2;
}

void perform_death_sequence() {
	load_standard_palettes();
}

void perform_level_end_sequence() {
	level.ending = 1;
	
	load_standard_palettes();	
	while (rescue.value) {
		if (rescue.value) {
			add_score(level.diver_score << 1);
			add_rescue(-1);

			wait_frames(20);
		}
		
		SMS_initSprites();	
		draw_actors();		
		SMS_finalizeSprites();
		SMS_waitForVBlank();
		SMS_copySpritestoSAT();
		
		draw_score_if_needed();
		draw_rescue_if_needed();
	}
	
	level.ending = 0;
}

char gameplay_loop() {
	int frame = 0;
	int fish_frame = 0;
	int torpedo_frame = 0;
	
	animation_delay = 0;
	
	set_score(0);
	set_rescue(0);
	set_life(4);
	
	level.number = 1;
	level.starting = 1;

	reset_actors_and_player();

	SMS_waitForVBlank();
	SMS_displayOff();
	SMS_disableLineInterrupt();

	SMS_loadPSGaidencompressedTiles(sprites_tiles_psgcompr, 0);
	
	draw_background();

	load_standard_palettes();

	clear_sprites();
	
	SMS_setLineInterruptHandler(&interrupt_handler);
	SMS_setLineCounter(180);
	SMS_enableLineInterrupt();

	SMS_displayOn();
		
	initialize_level();
	
	while(1) {	
		if (rescue.value == RESCUE_CHARS) {
			perform_level_end_sequence();
			level.number++;
			initialize_level();
			player1->active = 1;
		}

		if (!player1->active) {
			add_life(-1);
			reset_actors_and_player();
			level.starting = 1;
		}
		
		if (!life.value) {
			return STATE_GAMEOVER;
		}
	
		handle_player_input();
		handle_oxygen();
		
		if (!level.starting) {			
			handle_spawners();
			move_actors();
			check_collisions();
		}
		
		if (!player1->active) {
			perform_death_sequence();
		}
		
		SMS_initSprites();	

		draw_actors();		

		SMS_finalizeSprites();		

		SMS_waitForVBlank();
		SMS_copySpritestoSAT();

		draw_level_number();
		draw_score_if_needed();
		draw_rescue_if_needed();
		draw_life_if_needed();
				
		frame += 6;
		if (frame > 12) frame = 0;
		
		fish_frame += 4;
		if (fish_frame > 12) fish_frame = 0;
				
		torpedo_frame += 2;
		if (torpedo_frame > 4) torpedo_frame = 0;
		
		animation_delay--;
		if (animation_delay < 0) animation_delay = ANIMATION_SPEED;
	}
}

void print_number(char x, char y, unsigned int number, char extra_zero) {
	unsigned int base = 352 - 32;
	unsigned int remaining = number;
	
	if (extra_zero) {
		SMS_setNextTileatXY(x--, y);	
		SMS_setTile(base + '0');
	}
	
	while (remaining) {
		SMS_setNextTileatXY(x--, y);
		SMS_setTile(base + '0' + remaining % 10);
		remaining /= 10;
	}
}

char handle_gameover() {	
	return STATE_START;
}

char handle_title() {
	return STATE_GAMEPLAY;
}

void main() {
	char state = STATE_START;
	
	SMS_useFirstHalfTilesforSprites(1);
	SMS_setSpriteMode(SPRITEMODE_TALL);
	SMS_VDPturnOnFeature(VDPFEATURE_HIDEFIRSTCOL);
	SMS_VDPturnOnFeature(VDPFEATURE_LOCKHSCROLL);
	
	while (1) {
		switch (state) {
			
		case STATE_START:
			state = handle_title();
			break;
			
		case STATE_GAMEPLAY:
			state = gameplay_loop();
			break;
			
		case STATE_GAMEOVER:
			state = handle_gameover();
			break;
		}
	}
}

SMS_EMBED_SEGA_ROM_HEADER(9999,0); // code 9999 hopefully free, here this means 'homebrew'
SMS_EMBED_SDSC_HEADER(0,1, 2021,12,8, "Haroldo-OK\\2021", "Chicken Crossing",
  "Made for The Honest Jam III - https://itch.io/jam/honest-jam-3.\n"
  "Built using devkitSMS & SMSlib - https://github.com/sverx/devkitSMS");
