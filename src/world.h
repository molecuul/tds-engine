#pragma once
#include <stdint.h>

#include "object.h"
#include "vertex_buffer.h"
#include "quadtree.h"
#include "coord.h"

/* each block in the world is strictly 16 units tall, and 16 units wide. */

struct tds_world_hblock {
	int id;
	tds_bcp pos, dim; /* position and dimension are in world units, not block counts */
	struct tds_world_hblock* next;
	struct tds_vertex_buffer* vb;
};

struct tds_world_segment {
	tds_bcp a, b;
	tds_vec2 n;
	struct tds_world_segment* next, *prev;
};

/* it's a huge waste of.. everything to keep complete block data in memory. */

struct tds_world {
	struct tds_world_hblock* block_list_head, *block_list_tail;
	struct tds_world_segment* segment_list;
	struct tds_vertex_buffer* segment_vb;
	struct tds_quadtree* quadtree;
};

struct tds_world* tds_world_create(void);
void tds_world_free(struct tds_world* ptr);

void tds_world_init(struct tds_world* ptr); /* wipes active data from the world */
void tds_world_load_hblocks(struct tds_world* ptr, struct tds_world_hblock* block_list_head); /* the linked list is copied, please free it after passing */

int tds_world_get_overlap_fast(struct tds_world* ptr, struct tds_object* obj, int flag_req, int flag_or, int flag_not);
