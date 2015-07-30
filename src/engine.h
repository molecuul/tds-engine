#pragma once

/* The TDS engine subsystem manages the main loop, threading, and all subsystems. */

#include "display.h"
#include "texture_cache.h"
#include "sprite_cache.h"
#include "render.h"
#include "key_map.h"
#include "input.h"
#include "input_map.h"

struct tds_engine_desc {
	const char* config_filename;
	const char* map_filename;
	struct tds_key_map_template* game_input;
	int game_input_size;
};

struct tds_engine_state {
	char* mapname;
	float fps;
	int entity_count;
};

struct tds_engine_object_list {
	struct tds_object** buffer;
	int size;
};

struct tds_engine {
	struct tds_engine_desc desc;
	struct tds_engine_state state;

	struct tds_display* display_handle;
	struct tds_render* render_handle;
	struct tds_camera* camera_handle;
	struct tds_texture_cache* tc_handle;
	struct tds_sprite_cache* sc_handle;
	struct tds_handle_manager* object_buffer;
	struct tds_input* input_handle;
	struct tds_input_map* input_map_handle;
	struct tds_key_map* key_map_handle;

	int run_flag;
};

struct tds_engine* tds_engine_create(struct tds_engine_desc desc);
void tds_engine_free(struct tds_engine* ptr);

void tds_engine_run(struct tds_engine* ptr);
void tds_engine_flush_objects(struct tds_engine* ptr); /* destroys all objects in the buffer. */
void tds_engine_terminate(struct tds_engine* ptr); /* flags the engine to stop soon */

struct tds_object* tds_engine_get_object_by_type(struct tds_engine* ptr, const char* type);
struct tds_engine_object_list tds_engine_get_object_list_by_type(struct tds_engine* ptr, const char* type); /* Allocates a buffer. Must be freed after reading! */

void tds_engine_load_map(struct tds_engine* ptr, char* mapname);
void tds_engine_save_map(struct tds_engine* ptr, char* mapname);

struct tds_engine* tds_engine_global;