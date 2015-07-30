#pragma once

#include "../object.h"

#include "system.h"

struct tds_object_type** tds_object_type_get_list(void);
int tds_object_type_get_count(void);
struct tds_object_type* tds_object_type_get_by_type(const char* type_name);

/* We will encapulate it with functions to make it smaller in memory (avoid multiple definitions + static decl) */
