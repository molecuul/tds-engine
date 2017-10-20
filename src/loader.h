#pragma once

#include "engine.h"
#include "coord.h"
#include "world.h"

struct tds_loader {
	struct tds_world** world_buf;
	unsigned world_buf_len;
};

struct tds_loader* tds_loader_create(void);
void tds_loader_free(void);

int tds_loader_parse(struct tds_loader* state, struct tds_engine* eng, const char* filename);
