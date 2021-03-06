#include "engine.h"
#include "config.h"
#include "display.h"
#include "object.h"
#include "handle.h"
#include "memory.h"
#include "clock.h"
#include "sprite.h"
#include "sound_buffer.h"
#include "log.h"
#include "msg.h"
#include "yxml.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "objects/objects.h"

#define TDS_ENGINE_TIMESTEP 120.0f

struct tds_engine* tds_engine_global = NULL;

struct tds_engine* tds_engine_create(struct tds_engine_desc desc) {
	if (tds_engine_global) {
		tds_logf(TDS_LOG_CRITICAL, "Only one engine can exist!\n");
		return NULL;
	}

	struct tds_engine* output = tds_malloc(sizeof(struct tds_engine));

	tds_engine_global = output;

	srand(time(NULL));

	struct tds_display_desc display_desc;

	output->desc = desc;
	output->object_list = NULL;

	output->enable_update = output->enable_draw = 1;

	output->state.fps = 0.0f;
	output->state.entity_maxindex = 0;
	output->request_load = NULL;

	tds_logf(TDS_LOG_MESSAGE, "Initializing TDS engine..\n");

	struct tds_script* engine_conf = tds_script_create(desc.config_filename);
	tds_logf(TDS_LOG_MESSAGE, "Executed engine configuration.\n");

	tds_signal_init();
	tds_logf(TDS_LOG_MESSAGE, "Registered signal handlers.\n");

	output->profile_handle = tds_profile_create();
	tds_logf(TDS_LOG_MESSAGE, "Initialized engine profiler.\n");

	tds_profile_push(output->profile_handle, "Init sequence");

	output->stringdb_handle = tds_stringdb_create(desc.stringdb_filename);
	tds_logf(TDS_LOG_MESSAGE, "Initialized string database.\n");

	/* Display subsystem */
	display_desc.width = tds_script_get_var_int(engine_conf, "width", 640);
	display_desc.height = tds_script_get_var_int(engine_conf, "height", 480);
	display_desc.fs = tds_script_get_var_bool(engine_conf, "fullscreen", 0);
	display_desc.vsync = tds_script_get_var_int(engine_conf, "verticalsync", 0);
	display_desc.msaa = tds_script_get_var_int(engine_conf, "msaa", 0);

	tds_logf(TDS_LOG_MESSAGE, "Loaded display description. Video mode : %d by %d, FS %s, VSYNC %d intervals, MSAA %d\n", display_desc.width, display_desc.height, display_desc.fs ? "on" : "off", display_desc.vsync, display_desc.msaa);
	output->display_handle = tds_display_create(display_desc);
	tds_logf(TDS_LOG_MESSAGE, "Created display.\n");
	/* End display subsystem */

	output->tc_handle = tds_texture_cache_create();
	tds_logf(TDS_LOG_MESSAGE, "Initialized texture cache.\n");

	output->sc_handle = tds_sprite_cache_create();
	tds_logf(TDS_LOG_MESSAGE, "Initialized sprite cache.\n");

	output->sndc_handle = tds_sound_cache_create();
	tds_logf(TDS_LOG_MESSAGE, "Initialized sound cache.\n");

	output->otc_handle = tds_object_type_cache_create();
	tds_logf(TDS_LOG_MESSAGE, "Initialized object type cache.\n");

	output->object_buffer = tds_handle_manager_create(1024);
	tds_logf(TDS_LOG_MESSAGE, "Initialized object buffer.\n");

	output->ft_handle = tds_ft_create();
	tds_logf(TDS_LOG_MESSAGE, "Initialized FreeType2 context.\n");

	output->fc_handle = tds_font_cache_create();
	tds_logf(TDS_LOG_MESSAGE, "Initialized font cache.\n");

	output->camera_handle = tds_camera_create(output->display_handle);
	tds_camera_set(output->camera_handle, 10.0f, 0.0f, 0.0f);
	tds_logf(TDS_LOG_MESSAGE, "Initialized camera system.\n");

	output->render_handle = tds_render_create(output->camera_handle, output->object_buffer);
	tds_logf(TDS_LOG_MESSAGE, "Initialized render system.\n");

	output->render_handle->enable_bloom = tds_script_get_var_bool(engine_conf, "enable_bloom", 0);
	output->render_handle->enable_dynlights = tds_script_get_var_bool(engine_conf, "enable_dynlights", 1);

	output->render_flat_world_handle = tds_render_flat_create();
	tds_logf(TDS_LOG_MESSAGE, "Initialized flat rendering (world) system.\n");

	output->render_flat_overlay_handle = tds_render_flat_create();
	tds_logf(TDS_LOG_MESSAGE, "Initialized flat rendering (overlay) system.\n");

	output->input_handle = tds_input_create(output->display_handle);
	tds_logf(TDS_LOG_MESSAGE, "Initialized input system.\n");

	output->sound_manager_handle = tds_sound_manager_create();
	tds_logf(TDS_LOG_MESSAGE, "Initialized OpenAL context.\n");

	output->input_map_handle = tds_input_map_create(output->input_handle);
	tds_logf(TDS_LOG_MESSAGE, "Initialized input mapping system.\n");

	output->key_map_handle = tds_key_map_create(desc.game_input, desc.game_input_size);
	tds_logf(TDS_LOG_MESSAGE, "Initialized key mapping system.\n");

	output->block_map_handle = tds_block_map_create();
	tds_logf(TDS_LOG_MESSAGE, "Initialized block mapping subsystem.\n");

	output->effect_handle = tds_effect_create();
	tds_logf(TDS_LOG_MESSAGE, "Initialized effect subsystem.\n");

	output->bg_handle = tds_bg_create();
	tds_logf(TDS_LOG_MESSAGE, "Initialized background subsystem.\n");

	output->world_buffer_count = 0;

	for (int i = 0; i < TDS_MAX_WORLD_LAYERS; ++i) {
		output->world_buffer[i] = tds_world_create();
		tds_logf(TDS_LOG_MESSAGE, "Initialized world subsystem for layer %d.\n", i);
	}

	output->savestate_handle = tds_savestate_create();
	tds_savestate_set_index(output->savestate_handle, desc.save_index);
	tds_logf(TDS_LOG_MESSAGE, "Initialized savestate subsystem.\n");

	if (desc.func_load_sprites) {
		desc.func_load_sprites(output->sc_handle, output->tc_handle);
		tds_logf(TDS_LOG_MESSAGE, "Loaded sprites.\n");
	}

	if (desc.func_load_block_map) {
		desc.func_load_block_map(output->block_map_handle, output->tc_handle);
		tds_logf(TDS_LOG_MESSAGE, "Loaded block types.\n");
	}

	if (desc.func_load_sounds) {
		desc.func_load_sounds(output->sndc_handle);
		tds_logf(TDS_LOG_MESSAGE, "Loaded sounds.\n");
	}

	if (desc.func_load_fonts) {
		desc.func_load_fonts(output->fc_handle, output->ft_handle);
		tds_logf(TDS_LOG_MESSAGE, "Loaded fonts.\n");
	}

	output->module_container_handle = tds_module_container_create();
	tds_logf(TDS_LOG_MESSAGE, "Initialized module container subsystem.\n");

	output->font_debug = tds_font_cache_get(output->fc_handle, "debug");

	if (output->font_debug) {
		tds_logf(TDS_LOG_MESSAGE, "Loaded debug font.\n");
	} else {
		tds_logf(TDS_LOG_WARNING, "No debug font in cache! Some text might be missing.\n");
	}

	/* Here, we add the editor objects. */
	tds_load_editor_objects(output->otc_handle);

	if (desc.func_load_object_types) {
		desc.func_load_object_types(output->otc_handle);
		tds_logf(TDS_LOG_MESSAGE, "Loaded object types.\n");
	}

	if (desc.func_load_modules) {
		desc.func_load_modules(output->module_container_handle);
		tds_logf(TDS_LOG_MESSAGE, "Loaded gamespace modules.\n");
	}

	output->console_handle = tds_console_create();
	tds_logf(TDS_LOG_MESSAGE, "Initialized console.\n");

	output->enable_fps = 0;

	/* Free configs */
	tds_script_free(engine_conf);
	tds_logf(TDS_LOG_MESSAGE, "Done initializing everything.\n");
	tds_logf(TDS_LOG_MESSAGE, "Engine is ready to roll!\n");

	tds_profile_pop(output->profile_handle);

	if (desc.map_filename) {
		if (strcmp(desc.map_filename, "none")) {
			tds_logf(TDS_LOG_MESSAGE, "Loading initial map [%s].\n", desc.map_filename);
			tds_engine_load(output, desc.map_filename);
		} else {
			tds_logf(TDS_LOG_WARNING, "Not loading a map.\n");
		}
	}

	return output;
}

void tds_engine_free(struct tds_engine* ptr) {
	tds_logf(TDS_LOG_MESSAGE, "Freeing engine structure and subsystems.\n");

	if (ptr->object_list) {
		tds_free(ptr->object_list);
	}

	tds_engine_flush_objects(ptr);

	tds_profile_output(ptr->profile_handle);
	tds_profile_flush(ptr->profile_handle);

	tds_block_map_free(ptr->block_map_handle);

	for (int i = 0; i < TDS_MAX_WORLD_LAYERS; ++i) {
		tds_world_free(ptr->world_buffer[i]);
	}

	if (ptr->request_load) {
		tds_free(ptr->request_load);
	}

	if (ptr->state.mapname) {
		tds_free(ptr->state.mapname);
	}

	tds_input_free(ptr->input_handle);
	tds_input_map_free(ptr->input_map_handle);
	tds_key_map_free(ptr->key_map_handle);
	tds_render_free(ptr->render_handle);
	tds_render_flat_free(ptr->render_flat_world_handle);
	tds_render_flat_free(ptr->render_flat_overlay_handle);
	tds_bg_free(ptr->bg_handle);
	tds_camera_free(ptr->camera_handle);
	tds_display_free(ptr->display_handle);
	tds_texture_cache_free(ptr->tc_handle);
	tds_font_cache_free(ptr->fc_handle);
	tds_sprite_cache_free(ptr->sc_handle);
	tds_sound_cache_free(ptr->sndc_handle);
	tds_object_type_cache_free(ptr->otc_handle);
	tds_sound_manager_free(ptr->sound_manager_handle);
	tds_effect_free(ptr->effect_handle);
	tds_handle_manager_free(ptr->object_buffer);
	tds_console_free(ptr->console_handle);
	tds_savestate_free(ptr->savestate_handle);
	tds_stringdb_free(ptr->stringdb_handle);
	tds_module_container_free(ptr->module_container_handle);
	tds_ft_free(ptr->ft_handle);
	tds_profile_free(ptr->profile_handle);
	tds_free(ptr);
}

void tds_engine_run(struct tds_engine* ptr) {
	int running = ptr->run_flag = 1, accum_frames = 0;

	tds_logf(TDS_LOG_MESSAGE, "Starting engine mainloop.\n");
	tds_sound_manager_set_pos(ptr->sound_manager_handle, ptr->camera_handle->x, ptr->camera_handle->y);

	/* The game, like 'hunter' will use an accumulator-based approach with a fixed timestep.
	 * to support higher-refresh rate monitors, the timestep will be set to 144hz. */

	/* The game logic will update on a time-dependent basis, but the game will draw whenever possible. */

	tds_clock_point dt_point = tds_clock_get_point();
	double accumulator = 0.0f;
	double timestep_ms = 1000.0f / (double) TDS_ENGINE_TIMESTEP;

	unsigned long frame_count = 0;
	tds_clock_point init_point = tds_clock_get_point();

	const int fps_graph_cnt = 32;
	float fps_max = 0.0f, fps_min = 1000.0f, fps_graph[fps_graph_cnt];
	int fps_graph_write = 0;

	while (running && ptr->run_flag) {
		running &= !tds_display_get_close(ptr->display_handle);

		double delta_ms = tds_clock_get_ms(dt_point);
		dt_point = tds_clock_get_point();
		accumulator += delta_ms;

		frame_count++;

		/* We approximate the fps using the delta frame time. */
		ptr->state.fps = 1000.0f / delta_ms;

		float sum = 0.0f;
		for (int i = 0; i < fps_graph_cnt; ++i) {
			sum += fps_graph[i];
		}

		fps_min = (sum / fps_graph_cnt) - 200.0f;
		fps_max = (sum / fps_graph_cnt) + 200.0f;

		if (ptr->state.fps > fps_max) fps_max = ptr->state.fps;
		if (ptr->state.fps < fps_min) fps_min = ptr->state.fps;

		// Useful message for debugging frame delta timings.
		// tds_logf(TDS_LOG_MESSAGE, "frame : accum = %f ms, delta_ms = %f ms, timestep = %f\n", accumulator, delta_ms, timestep_ms);

		ptr->state.entity_maxindex = ptr->object_buffer->max_index;

		tds_display_update(ptr->display_handle);

		tds_profile_push(ptr->profile_handle, "Update cycle");

		while (accumulator >= timestep_ms) {
			accumulator -= timestep_ms;
			accum_frames++;

			/* Run game update logic. */
			/* Even if updating is disabled, we still want to run down the accumulator. */

			tds_input_update(ptr->input_handle);

			if (ptr->enable_update) {
				for (int i = 0; i < ptr->object_buffer->max_index; ++i) {
					struct tds_object* target = (struct tds_object*) ptr->object_buffer->buffer[i].data;

					if (!target) {
						continue;
					}

					tds_object_update(target);
				}

				tds_effect_update(ptr->effect_handle);
				tds_module_container_update(ptr->module_container_handle);
			}
		}

		tds_profile_pop(ptr->profile_handle);

		/* Run game draw logic. */
		tds_render_flat_clear(ptr->render_flat_world_handle);
		tds_render_flat_clear(ptr->render_flat_overlay_handle);

		tds_render_clear(ptr->render_handle); /* We clear before executing the draw functions, otherwise the text buffer would be destroyed */
		tds_profile_push(ptr->profile_handle, "Draw event cycle");

		if (ptr->enable_draw) {
			for (int i = 0; i < ptr->object_buffer->max_index; ++i) {
				struct tds_object* target = (struct tds_object*) ptr->object_buffer->buffer[i].data;

				if (!target) {
					continue;
				}

				tds_object_draw(target);
			}
		}

		tds_profile_pop(ptr->profile_handle);
		tds_module_container_draw(ptr->module_container_handle);
		tds_console_draw(ptr->console_handle);

		/* if fps enabled, render some performance info */
		if (ptr->enable_fps) {
			tds_render_flat_set_mode(ptr->render_flat_overlay_handle, TDS_RENDER_COORD_REL_SCREENSPACE);
			tds_render_flat_set_color(ptr->render_flat_overlay_handle, 1.0f, 1.0f, 1.0f, 1.0f);

			char fps_buf[12] = {0};
			snprintf(fps_buf, sizeof fps_buf, "fps %d", (int) ptr->state.fps);

			char accum_buf[24] = {0};
			snprintf(accum_buf, sizeof accum_buf, "accumulator ms %d", (int) accumulator);

			tds_render_flat_text(ptr->render_flat_overlay_handle, ptr->font_debug, fps_buf, strlen(fps_buf), -1.0f, -0.9f, TDS_RENDER_LALIGN, NULL);

			tds_render_flat_set_color(ptr->render_flat_overlay_handle, 0.0f, 1.0f, 1.0f, 1.0f);
			tds_render_flat_text(ptr->render_flat_overlay_handle, ptr->font_debug, accum_buf, strlen(accum_buf), -1.0f, -0.95f, TDS_RENDER_LALIGN, NULL);

			/*  render a graph between -0.75 and -0.85 (v) and -1.0 and -0.8 (h) */
			fps_graph[fps_graph_write] = ptr->state.fps;

			/* first, render everything after the write pos */
			int wp = 0;
			for (int i = fps_graph_write + 1; i < fps_graph_cnt - 1; ++i) {
				tds_render_flat_line(ptr->render_flat_overlay_handle, -1.0f + (wp / (float) fps_graph_cnt) * 0.2f, -0.85 + ((fps_graph[i] - fps_min) / (fps_max - fps_min)) * 0.1f, -1.0f + ((wp + 1) / (float) fps_graph_cnt) * 0.2f, -0.85 + ((fps_graph[i + 1] - fps_min) / (fps_max - fps_min)) * 0.1f);

				wp++;
			}

			for (int i = 0; i <= fps_graph_write; ++i) {
				tds_render_flat_line(ptr->render_flat_overlay_handle, -1.0f + (wp / (float) fps_graph_cnt) * 0.2f, -0.85 + ((fps_graph[i] - fps_min) / (fps_max - fps_min)) * 0.1f, -1.0f + ((wp + 1) / (float) fps_graph_cnt) * 0.2f, -0.85 + ((fps_graph[i + 1] - fps_min) / (fps_max - fps_min)) * 0.1f);

				wp++;
			}

			if (++fps_graph_write >= fps_graph_cnt) fps_graph_write = 0;
		}

		tds_profile_push(ptr->profile_handle, "Render process");
		tds_render_draw(ptr->render_handle, ptr->world_buffer, ptr->world_buffer_count, ptr->render_flat_world_handle, ptr->render_flat_overlay_handle);
		tds_profile_pop(ptr->profile_handle);
		tds_display_swap(ptr->display_handle);

		tds_render_clear_lights(ptr->render_handle);

		/* the frame is over. if any loads were requested, we do that now. */
		if (ptr->request_load) {
			tds_logf(TDS_LOG_DEBUG, "acknowledging request to load [%s]\n", ptr->request_load);
			tds_engine_load(ptr, ptr->request_load);
			tds_free(ptr->request_load);
			ptr->request_load = NULL;
		}
	}

	tds_logf(TDS_LOG_MESSAGE, "Finished engine mainloop. Average framerate: %f FPS [%d frames in %f s]\n", (float) frame_count / ((float) tds_clock_get_ms(init_point) / 1000.0f), frame_count, tds_clock_get_ms(init_point) / 1000.0f);
}

void tds_engine_flush_objects(struct tds_engine* ptr) {
	for (int i = 0; i < ptr->object_buffer->max_index; ++i) {
		if (ptr->object_buffer->buffer[i].data) {
			tds_object_free(ptr->object_buffer->buffer[i].data);
		}
	}

	tds_bg_flush(ptr->bg_handle);
	tds_effect_flush(ptr->effect_handle);
}

struct tds_object* tds_engine_get_object_by_type(struct tds_engine* ptr, const char* typename) {
	for (int i = 0; i < ptr->object_buffer->max_index; ++i) {
		if (!ptr->object_buffer->buffer[i].data) {
			continue;
		}

		if (!strcmp(((struct tds_object*) ptr->object_buffer->buffer[i].data)->type_name, typename)) {
			return ptr->object_buffer->buffer[i].data;
		}
	}

	return NULL;
}

struct tds_engine_object_list tds_engine_get_object_list_by_type(struct tds_engine* ptr, const char* typename) {
	if (ptr->object_list) {
		tds_free(ptr->object_list);
		ptr->object_list = NULL;
	}

	struct tds_engine_object_list output;

	output.size = 0;

	for (int i = 0; i < ptr->object_buffer->max_index; ++i) {
		if (!ptr->object_buffer->buffer[i].data) {
			continue;
		}

		if (!strcmp(((struct tds_object*) ptr->object_buffer->buffer[i].data)->type_name, typename)) {
			ptr->object_list = tds_realloc(ptr->object_list, sizeof(struct tds_object*) * ++output.size);
			ptr->object_list[output.size - 1] = (struct tds_object*) ptr->object_buffer->buffer[i].data;
		}
	}

	output.buffer = ptr->object_list;
	return output;
}

void tds_engine_object_foreach(struct tds_engine* ptr, void* data, void (*callback)(void* data, struct tds_object* object)) {
	if (!callback) {
		return;
	}

	for (int i = 0; i < ptr->object_buffer->max_index; ++i) {
		if (ptr->object_buffer->buffer[i].data) {
			callback(data, ptr->object_buffer->buffer[i].data);
		}
	}
}

void tds_engine_terminate(struct tds_engine* ptr) {
	ptr->run_flag = 0;
}

void tds_engine_load(struct tds_engine* ptr, const char* mapname) {
	char* str_filename = tds_malloc(strlen(mapname) + strlen(TDS_MAP_PREFIX) + 1);

	memcpy(str_filename, TDS_MAP_PREFIX, strlen(TDS_MAP_PREFIX));
	memcpy(str_filename + strlen(TDS_MAP_PREFIX), mapname, strlen(mapname));

	str_filename[strlen(TDS_MAP_PREFIX) + strlen(mapname)] = 0;
	tds_logf(TDS_LOG_DEBUG, "Loading map [%s] (%s)\n", str_filename, mapname);

	FILE* fd = fopen(str_filename, "r");

	if (!fd) {
		tds_logf(TDS_LOG_WARNING, "Failed to load map %s.\n", mapname);
		tds_free(str_filename);
		return;
	}

	/* the map actually exists -- NOW we destroy everything. */

	if (ptr->state.mapname) {
		tds_free(ptr->state.mapname);
		ptr->state.mapname = NULL;
	}

	ptr->state.mapname = tds_malloc(strlen(mapname) + 1);
	memcpy(ptr->state.mapname, mapname, strlen(mapname));
	ptr->state.mapname[strlen(mapname)] = 0;

	tds_engine_flush_objects(ptr);
	tds_effect_flush(ptr->effect_handle);
	tds_bg_flush(ptr->bg_handle);

	for (int i = 0; i < TDS_MAX_WORLD_LAYERS; ++i) {
		tds_world_free(ptr->world_buffer[i]);
		ptr->world_buffer[i] = tds_world_create();
		tds_logf(TDS_LOG_MESSAGE, "Initialized world subsystem for layer %d.\n", i);
	}

	ptr->world_buffer_count = 0;

	yxml_t* ctx = tds_malloc(sizeof(yxml_t) + TDS_LOAD_BUFFER_SIZE); // We hide the buffer with the YXML context
	yxml_init(ctx, ctx + 1, TDS_LOAD_BUFFER_SIZE);

	int in_layer = 0, in_object = 0, in_parameter = 0, in_data = 0;

	struct tds_object* cur_object = NULL;
	struct tds_object_param* cur_object_param = NULL;

	char obj_type_buf[TDS_LOAD_ATTR_SIZE + 1] = {0};
	char obj_x_buf[TDS_LOAD_ATTR_SIZE + 1] = {0};
	char obj_y_buf[TDS_LOAD_ATTR_SIZE + 1] = {0};
	char obj_width_buf[TDS_LOAD_ATTR_SIZE + 1] = {0};
	char obj_height_buf[TDS_LOAD_ATTR_SIZE + 1] = {0};
	char obj_angle_buf[TDS_LOAD_ATTR_SIZE + 1] = {0};
	char obj_visible_buf[TDS_LOAD_ATTR_SIZE + 1] = {0};

	char* target_attr = obj_type_buf;

	char data_encoding_buf[TDS_LOAD_ATTR_SIZE + 1] = {0};

	char world_width_buf[TDS_LOAD_ATTR_SIZE + 1] = {0};
	char world_height_buf[TDS_LOAD_ATTR_SIZE + 1] = {0};

	int world_width = 1, world_height = 1;

	char world_read_buf[TDS_LOAD_WORLD_SIZE + 1] = {0};
	int world_read_len = 0;
	int world_read_pos = 0;

	uint8_t* id_buffer = NULL;

	char prop_name_buf[TDS_LOAD_ATTR_SIZE + 1] = {0};
	char prop_val_buf[TDS_LOAD_ATTR_SIZE + 1] = {0};

	int dont_load_world = 0;

	char c_char = 0;
	while ((c_char = fgetc(fd)) != EOF) {

		if (!c_char) {
			break;
		}

		yxml_ret_t r = yxml_parse(ctx, c_char);

		if (r < 0) {
			tds_logf(TDS_LOG_WARNING, "yxml parsing error while loading %s.\n", str_filename);
			tds_free(str_filename);
			tds_free(ctx);
			return;
		}

		switch (r) {
		case YXML_ELEMSTART:
			if (!strcmp(ctx->elem, "object")) {
				in_object = 1;
			}
			if (!strcmp(ctx->elem, "layer")) {
				in_layer = 1;
			}
			if (!strcmp(ctx->elem, "data")) {
				in_data = 1;
			}
			if (!strcmp(ctx->elem, "property")) {
				in_parameter = 1;
			}
			break;
		case YXML_ATTRSTART:
			target_attr = NULL;

			if (in_object) {
				if (!strcmp(ctx->attr, "type")) {
					target_attr = obj_type_buf;
				}

				if (!strcmp(ctx->attr, "x")) {
					target_attr = obj_x_buf;
				}

				if (!strcmp(ctx->attr, "y")) {
					target_attr = obj_y_buf;
				}

				if (!strcmp(ctx->attr, "width")) {
					target_attr = obj_width_buf;
				}

				if (!strcmp(ctx->attr, "height")) {
					target_attr = obj_height_buf;
				}

				if (!strcmp(ctx->attr, "visible")) {
					target_attr = obj_visible_buf;
				}

				if (!strcmp(ctx->attr, "angle")) {
					target_attr = obj_angle_buf;
				}
			}

			if (in_layer && !in_data) {
				if (!strcmp(ctx->attr, "width")) {
					target_attr = world_width_buf;
				}

				if (!strcmp(ctx->attr, "height")) {
					target_attr = world_height_buf;
				}
			}

			if (in_data) {
				if (!strcmp(ctx->attr, "encoding")) {
					target_attr = data_encoding_buf;
				}
			}

			if (in_parameter) {
				if (!strcmp(ctx->attr, "name"))	{
					target_attr = prop_name_buf;
				}

				if (!strcmp(ctx->attr, "value")) {
					target_attr = prop_val_buf;
				}
			}
			break;
		case YXML_ATTRVAL:
			if (!target_attr) {
				break;
			}

			if (strlen(target_attr) >= TDS_LOAD_ATTR_SIZE) {
				tds_logf(TDS_LOG_WARNING, "Attribute value too large, truncating! %s=%s..\n", ctx->attr, target_attr);
				break;
			}

			target_attr[strlen(target_attr)] = *(ctx->data);
			break;
		case YXML_ATTREND:
			if (in_data) {
				if (strcmp(data_encoding_buf, "csv")) {
					tds_logf(TDS_LOG_WARNING, "World data should be encoded as CSV. [%s]\n", data_encoding_buf);
					dont_load_world = 1;
				}
			}
			break;
		case YXML_CONTENT:
			if (in_data) {
				memset(data_encoding_buf, 0, sizeof data_encoding_buf / sizeof *data_encoding_buf);

				if (!id_buffer) {
					world_width = strtol(world_width_buf, NULL, 10);
					world_height = strtol(world_width_buf, NULL, 10);

					id_buffer = tds_malloc(world_width * world_height * sizeof *id_buffer);
				}

				if (world_read_len >= (sizeof world_read_buf / sizeof *world_read_buf) - 1) {
					tds_logf(TDS_LOG_WARNING, "World data too large!\n");
					break;
				}

				if (*(ctx->data) == ',') {
					/* Push the current readbuf to the id buffer and reset the readbuf. */
					int tx = world_read_pos % world_width, ty = (world_height - 1) - (world_read_pos / world_width);
					++world_read_pos;

					if (world_read_pos >= world_width * world_height) {
						tds_logf(TDS_LOG_DEBUG, "World read pos extended past the end of the world.\n");
						memset(world_read_buf, 0, sizeof world_read_buf / sizeof *world_read_buf);
						world_read_len = 0;
						break;
					}

					id_buffer[ty * world_width + tx] = strtol(world_read_buf, NULL, 10);

					memset(world_read_buf, 0, sizeof world_read_buf / sizeof *world_read_buf);
					world_read_len = 0;
				} else {
					world_read_buf[world_read_len++] = *(ctx->data);
				}
			}
			break;
		case YXML_ELEMEND:
			if (in_object && !in_parameter) {
				in_object = 0;

				struct tds_object_type* type_ptr = tds_object_type_cache_get(ptr->otc_handle, obj_type_buf);

				if (!type_ptr) {
					tds_logf(TDS_LOG_WARNING, "Unknown typename in map file [%s]!\n", type_ptr);

					memset(obj_type_buf, 0, sizeof obj_type_buf / sizeof *obj_type_buf);
					memset(obj_visible_buf, 0, sizeof obj_visible_buf / sizeof *obj_visible_buf);
					memset(obj_angle_buf, 0, sizeof obj_angle_buf / sizeof *obj_angle_buf);
					memset(obj_x_buf, 0, sizeof obj_x_buf / sizeof *obj_x_buf);
					memset(obj_y_buf, 0, sizeof obj_y_buf / sizeof *obj_y_buf);
					memset(obj_width_buf, 0, sizeof obj_width_buf / sizeof *obj_width_buf);
					memset(obj_height_buf, 0, sizeof obj_height_buf / sizeof *obj_height_buf);

					cur_object_param = NULL;
					break;
				}

				float map_x = strtof(obj_x_buf, NULL), map_y = strtof(obj_y_buf, NULL);
				float map_block_size = TDS_WORLD_BLOCK_SIZE * 32.0f;
				float map_width = map_block_size * world_width;
				float map_height = map_block_size * world_height;
				float game_width = TDS_WORLD_BLOCK_SIZE * world_width;
				float game_height = TDS_WORLD_BLOCK_SIZE * world_height;
				float real_width = (strtof(obj_width_buf, NULL) / map_width) * game_width;
				float real_height = (strtof(obj_height_buf, NULL) / map_height) * game_height;
				float real_x = -game_width / 2.0f + (game_width * (map_x / map_width)) + (real_width / 2.0f);
				float real_y = -game_height / 2.0f + (game_height * ((map_height - map_y) / map_height)) - (real_height / 2.0f);

				float ratio_tlx = map_x / map_width;
				float ratio_tly = map_y / map_height;

				real_x = (-game_width / 2.0f) + ((game_width * ratio_tlx) + (real_width / 2.0f));
				real_y = (game_height / 2.0f) - ((game_height * ratio_tly) + (real_height / 2.0f));

				tds_logf(TDS_LOG_DEBUG, "Constructing object of type [%s] (map_x %f, map_y %f, map_block_size %f, map_width %f, map_height %f, game_width %f, game_height %f, real_width %f, real_height %f, real_x %f, real_y %f\n", obj_type_buf, map_x, map_y, map_block_size, map_width, map_height, game_width, game_height, real_width, real_height, real_x, real_y);

				cur_object = tds_object_create(type_ptr, ptr->object_buffer, ptr->sc_handle, real_x, real_y, 0.0f, cur_object_param);

				cur_object->cbox_width = real_width;
				cur_object->cbox_height = real_height;

				cur_object->visible = strcmp(obj_visible_buf, "0") ? 1 : 0;
				cur_object->angle = strtof(obj_angle_buf, NULL) * 3.141f / 180.0f;

				memset(obj_type_buf, 0, sizeof obj_type_buf / sizeof *obj_type_buf);
				memset(obj_visible_buf, 0, sizeof obj_visible_buf / sizeof *obj_visible_buf);
				memset(obj_angle_buf, 0, sizeof obj_angle_buf / sizeof *obj_angle_buf);
				memset(obj_x_buf, 0, sizeof obj_x_buf / sizeof *obj_x_buf);
				memset(obj_y_buf, 0, sizeof obj_y_buf / sizeof *obj_y_buf);
				memset(obj_width_buf, 0, sizeof obj_width_buf / sizeof *obj_width_buf);
				memset(obj_height_buf, 0, sizeof obj_height_buf / sizeof *obj_height_buf);

				cur_object_param = NULL;
			} else if (in_parameter) {
				in_parameter = 0;

				struct tds_object_param* next_param = tds_malloc(sizeof *next_param);

				next_param->next = cur_object_param;
				cur_object_param = next_param;

				switch (prop_name_buf[0]) {
				default:
					tds_logf(TDS_LOG_WARNING, "Invalid type prefix [%c] in object parameter; default to int\n", prop_name_buf[0]);
				case 'i':
					next_param->type = TDS_PARAM_INT;
					next_param->ipart = strtol(prop_val_buf, NULL, 10);
					break;
				case 'u':
					next_param->type = TDS_PARAM_UINT;
					next_param->upart = strtol(prop_val_buf, NULL, 10);
					break;
				case 'f':
					next_param->type = TDS_PARAM_FLOAT;
					next_param->fpart = strtof(prop_val_buf, NULL);
					break;
				case 's':
					{
						next_param->type = TDS_PARAM_STRING;
						int srclen = strlen(prop_val_buf), writelen = srclen;
						if (srclen > TDS_PARAM_VALSIZE) {
							tds_logf(TDS_LOG_WARNING, "Parameter string longer than %d. Truncating..\n", TDS_PARAM_VALSIZE);
							writelen = TDS_PARAM_VALSIZE;
						}
						memcpy(next_param->spart, prop_val_buf, writelen);
					}
					break;
				}

				next_param->key = strtol(prop_name_buf + 1, NULL, 10);
				tds_logf(TDS_LOG_DEBUG, "Created object parameter with type %c, key %d, valbuf [%s], namebuf [%s]\n", prop_name_buf[0], next_param->key, prop_val_buf, prop_name_buf);

				memset(prop_name_buf, 0, sizeof prop_name_buf / sizeof *prop_name_buf);
				memset(prop_val_buf, 0, sizeof prop_val_buf / sizeof *prop_val_buf);
			} else if (in_layer && in_data) {
				in_data = 0;
			} else if (in_layer && !in_data) {
				memset(world_read_buf, 0, sizeof world_read_buf / sizeof *world_read_buf);
				world_read_len = world_read_pos = 0;

				if (dont_load_world) {
					break;
				}

				in_layer = 0;

				/* The block IDs are stored now in id_buffer, with the correct winding order. */

				if (ptr->world_buffer_count >= TDS_MAX_WORLD_LAYERS) {
					tds_logf(TDS_LOG_WARNING, "There were more world layers in the map than allowed (max: %d) -- discarding extra layers\n", TDS_MAX_WORLD_LAYERS);
					break;
				}

				tds_world_init(ptr->world_buffer[ptr->world_buffer_count], world_width, world_height);
				tds_world_load(ptr->world_buffer[ptr->world_buffer_count++], id_buffer, world_width, world_height);

				memset(id_buffer, 0, sizeof(id_buffer[0]) * world_width * world_height);
				memset(data_encoding_buf, 0, sizeof data_encoding_buf / sizeof *data_encoding_buf);
			}
			break;
		default:
			break;
		}
	}

	if (id_buffer) {
		tds_free(id_buffer);
	}

	yxml_ret_t ret = yxml_eof(ctx);

	if (ret < 0) {
		tds_logf(TDS_LOG_WARNING, "yxml reported incorrectly formatted map file at EOF!\n");
	}

	tds_free(ctx);
	tds_free(str_filename);
	tds_engine_broadcast(ptr, TDS_MSG_MAP_READY, 0);
}

void tds_engine_request_load(struct tds_engine* ptr, const char* request_load) {
	tds_logf(TDS_LOG_DEBUG, "requested load for [%s]\n", request_load);

	ptr->request_load = tds_realloc(ptr->request_load, strlen(request_load) + 1);
	memcpy(ptr->request_load, request_load, strlen(request_load));
	ptr->request_load[strlen(request_load)] = 0;
}

void tds_engine_destroy_objects(struct tds_engine* ptr, const char* type_name) {
	for (int i = 0; i < ptr->object_buffer->max_index; ++i) {
		struct tds_object* cur = ptr->object_buffer->buffer[i].data;

		if (!cur) {
			continue;
		}

		if (!strcmp(cur->type_name, type_name)) {
			tds_object_free(cur);
		}
	}
}

void tds_engine_broadcast(struct tds_engine* ptr, int msg, void* param) {
	tds_module_container_broadcast(ptr->module_container_handle, msg, param);

	for (int i = 0; i < ptr->object_buffer->max_index; ++i) {
		struct tds_object* cur = ptr->object_buffer->buffer[i].data;

		if (!cur) {
			continue;
		}

		tds_object_msg(cur, NULL, msg, param);
	}
}

struct tds_world* tds_engine_get_foreground_world(struct tds_engine* ptr) {
	if (!ptr->world_buffer_count) {
		tds_logf(TDS_LOG_WARNING, "No world defined, cannot give a foreground to the caller.\n");
		return NULL;
	}

	return ptr->world_buffer[ptr->world_buffer_count - 1];
}
