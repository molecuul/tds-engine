#include "stringdb.h"
#include "log.h"
#include "memory.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct tds_string_format_fields tds_string_format_field_counts[] = {
	{TDS_STRING_FORMAT_TYPE_COLOR, 3},
	{TDS_STRING_FORMAT_TYPE_SHAKE, 1},
	{TDS_STRING_FORMAT_TYPE_WAVE,  3},
	{TDS_STRING_FORMAT_TYPE_END,   0},
};

struct tds_stringdb* tds_stringdb_create(const char* filename) {
	struct tds_stringdb* output = tds_malloc(sizeof *output);

	output->entry_count = 0;
	output->entries = NULL;

	output->error_entry.data = "ERROR (entry)";
	output->error_entry.len = strlen(output->error_entry.data);

	output->error_offset.data = "ERROR (offset)";
	output->error_offset.len = strlen(output->error_offset.data);

	output->error_empty.data = "ERROR (no strings)";
	output->error_empty.len = strlen(output->error_empty.data);

	char* filename_full = tds_malloc(strlen(filename) + strlen(TDS_STRINGDB_PREFIX) + 1);

	memcpy(filename_full, TDS_STRINGDB_PREFIX, strlen(TDS_STRINGDB_PREFIX));
	memcpy(filename_full + strlen(TDS_STRINGDB_PREFIX), filename, strlen(filename));

	filename_full[strlen(TDS_STRINGDB_PREFIX) + strlen(filename)] = 0;

	tds_logf(TDS_LOG_MESSAGE, "Loading string database from [%s]\n", filename_full);

	FILE* fd = fopen(filename_full, "r");

	if (!fd) {
		tds_logf(TDS_LOG_CRITICAL, "Failed to open string database [%s] for reading.\n", filename_full);
		return NULL;
	}

	tds_free(filename_full);

	char readbuf[TDS_STRINGDB_BUFLEN] = {0}, *readpos = readbuf;

	struct tds_string_db_entry* cur_entry = NULL;
	struct tds_string_db_offset_entry* cur_offset = NULL;

	while (memset(readbuf, 0, sizeof readbuf / sizeof *readbuf) && fgets(readbuf, sizeof readbuf / sizeof *readbuf, fd)) {
		readbuf[strcspn(readbuf, "\n")] = 0;

		switch(*readbuf) {
		case '@':
			/* New string db entry. We allocate a structure on the entries list and start reading indices. */
			readpos = readbuf + 1;

			cur_entry = tds_malloc(sizeof *cur_entry);

			cur_entry->id = tds_malloc(strlen(readpos));
			cur_entry->id_len = strlen(readpos);
			memcpy(cur_entry->id, readpos, cur_entry->id_len);

			cur_entry->next = output->entries;
			output->entries = cur_entry;
			output->entry_count++;

			cur_entry->offset_count = 0;
			cur_entry->offsets = NULL;
			cur_offset = NULL;
			break;
		case '#':
			readpos = readbuf + 1;

			if (!cur_entry) {
				tds_logf(TDS_LOG_WARNING, "Offset was specified but there was no associated string entry. Ignoring.\n");
				break;
			}

			cur_offset = tds_malloc(sizeof *cur_offset);
			cur_offset->offset_id = strtol(readpos, NULL, 0);

			cur_offset->string_count = 0;
			cur_offset->strings = NULL;

			cur_offset->next = cur_entry->offsets;
			cur_entry->offsets = cur_offset;
			cur_entry->offset_count++;
			break;
		case ':':
			readpos = readbuf + 1;

			if (!cur_offset) {
				tds_logf(TDS_LOG_WARNING, "String value given but there was no associated offset entry. Ignoring.\n");
				break;
			}

			struct tds_string_db_offset_entry_string* new_string_entry = tds_malloc(sizeof *new_string_entry);

			{
				int readpos_len = strlen(readpos);
				new_string_entry->str.data = tds_malloc(readpos_len + 1); /* We will allocate more memory than necessary if there are any format strings, but this saves us reallocation overhead. */
				new_string_entry->str.len = 0;

				for (int i = 0; i < readpos_len; ++i) {
					if (readpos[i] == TDS_STRING_FORMAT_SPEC) {
						if (++i >= readpos_len) {
							break;
						}

						tds_logf(TDS_LOG_DEBUG, "started format i %d -> readpos (type) %c\n", i, readpos[i]);

						struct tds_string_format* new_format = tds_malloc(sizeof *new_format);
						new_format->type = readpos[i];
						new_format->pos = new_string_entry->str.len;

						int fields = -1;
						for (int j = 0; j < sizeof tds_string_format_field_counts / sizeof *tds_string_format_field_counts; ++j) {
							if (tds_string_format_field_counts[j].type == new_format->type) {
								fields = tds_string_format_field_counts[j].fields;
								break;
							}
						}

						if (fields < 0) {
							tds_free(new_format);
							tds_logf(TDS_LOG_WARNING, "Unknown format specifier '%c' in string.\n", new_format->type);
							break;
						}

						++i;

						if (i + fields * 2 > readpos_len) {
							tds_free(new_format);
							tds_logf(TDS_LOG_WARNING, "Insufficient length for reading arguments to format string.\n");
							break;
						}

						for (int j = 0; j < fields; ++j) {
							char cur_field[3] = {0};

							cur_field[0] = readpos[i++];
							cur_field[1] = readpos[i++];

							new_format->fields[j] = strtol(cur_field, NULL, 16);
						}

						new_format->next = new_string_entry->str.formats;
						new_string_entry->str.formats = new_format;

						i--; /* Correct for last field having incremented the character. */
						continue;
					}

					if (i >= readpos_len) {
						break;
					}

					new_string_entry->str.data[new_string_entry->str.len++] = readpos[i];
				}

				new_string_entry->str.data[new_string_entry->str.len] = 0;

				new_string_entry->next = cur_offset->strings;
				cur_offset->strings = new_string_entry;
				cur_offset->string_count++;
			}

			break;
		case '!':
			break;
		default:
			tds_logf(TDS_LOG_WARNING, "Invalid format header '%c' in string database.\n", readbuf[0]);
			break;
		}
	}

	fclose(fd);

	tds_logf(TDS_LOG_MESSAGE, "Loaded %d string groups.\n", output->entry_count);
	return output;
}

void tds_stringdb_free(struct tds_stringdb* ptr) {
	struct tds_string_db_entry* cur_entry = ptr->entries, *tmp_entry = NULL;

	while (cur_entry) {
		struct tds_string_db_offset_entry* cur_offset = cur_entry->offsets, *tmp_offset = NULL;

		while (cur_offset) {
			struct tds_string_db_offset_entry_string* cur_string = cur_offset->strings, *tmp_string = NULL;

			while (cur_string) {
				struct tds_string_format* cur_format = cur_string->str.formats, *tmp_format = NULL;

				while (cur_format) {
					tmp_format = cur_format->next;
					tds_free(cur_format);
					cur_format = tmp_format;
				}

				tmp_string = cur_string->next;
				tds_free(cur_string->str.data);
				tds_free(cur_string);
				cur_string = tmp_string;
			}

			tmp_offset = cur_offset->next;
			tds_free(cur_offset);
			cur_offset = tmp_offset;
		}

		tmp_entry = cur_entry->next;
		tds_free(cur_entry->id);
		tds_free(cur_entry);
		cur_entry = tmp_entry;
	}

	tds_free(ptr);
}

struct tds_string* tds_stringdb_get(struct tds_stringdb* ptr, char* id, int id_len, int index) {
	struct tds_string_db_entry* cur_entry = ptr->entries;

	while (cur_entry) {
		if (id_len != cur_entry->id_len) {
			cur_entry = cur_entry->next;
			continue;
		}

		if (strncmp(id, cur_entry->id, id_len)) {
			cur_entry = cur_entry->next;
			continue;
		}

		struct tds_string_db_offset_entry* cur_offset = cur_entry->offsets;

		while (cur_offset) {
			if (cur_offset->offset_id != index) {
				cur_offset = cur_offset->next;
				continue;
			}

			/* offset matches. find a string! */

			if (!cur_offset->string_count) {
				tds_logf(TDS_LOG_WARNING, "Offset %d in string group [%.*s] is empty.\n", index, id_len, id);
				return &ptr->error_empty;
			}

			struct tds_string_db_offset_entry_string* cur_string = cur_offset->strings;
			int choice_index = rand() % cur_offset->string_count;

			for (int i = 0; i < choice_index && cur_string; ++i) {
				cur_string = cur_string->next;

				if (!cur_string) {
					tds_logf(TDS_LOG_CRITICAL, "Offset string count doesn't match linked list size, something is wrong.\n");
					break;
				}
			}

			return &cur_string->str;
		}

		tds_logf(TDS_LOG_WARNING, "Failed to locate string offset %d for group [%.*s] in database.\n", index, id_len, id);
		return &ptr->error_offset;
	}

	tds_logf(TDS_LOG_WARNING, "Failed to locate string group [%.*s] in database.\n", id_len, id);
	return &ptr->error_entry;
}
