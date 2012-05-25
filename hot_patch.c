/*
 * hot_patch.c for hot_patch
 * by lenorm_f
 */

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "hot_patch.h"

#define BUF_SIZE 256
#define TYPE_BUF_SIZE 10
#define VALUE_BUF_SIZE 20

#define IS_SEP(x) ((x) == ' ' || (x) == '\t')

#ifdef DEBUG_HP
#define DBG_OUT(fmt, arg...) fprintf(stderr, "[DBG] " fmt "\n", ##arg)
#else
#define DBG_OUT(fmt, arg...)
#endif

// Available types
typedef enum {
	TYPE_UNKNOWN = -1,
	TYPE_INT,
	TYPE_FLOAT,
} eType;

// Value holder, handles all available types
typedef struct hp_value_s {
	eType type;

	union {
		int integer;
		float floating_integer;
	} value;
} hp_value_t;

// Tweakable variables cache
typedef struct hp_vars_list_s {
	char path[BUF_SIZE];
	hp_id_t id;

	hp_value_t value;

	struct hp_vars_list_s *next;
} hp_vars_list_t;

// Files (compilation units) cache
typedef struct hp_files_list_s {
	char path[BUF_SIZE];
	unsigned int mod_timestamp;

	struct hp_files_list_s *next;
} hp_files_list_t;

#ifdef DEBUG

static hp_vars_list_t *hp_tweakable_vars = NULL;
static hp_files_list_t *hp_tweakable_files = NULL;

/*
 * Local utils functions
 */

// Return the index of the first char that is not a
// separator (space/tabulation) in the string
static int left_strip_index(char const *s) {
	int i;
	for (i = 0; s[i]; ++i)
		if (!IS_SEP(s[i]))
			return i;

	return -1;
}

// Copy the input string until a given character is met
static unsigned int consume_char_until(char const *src, char *dst, unsigned int dst_size, char until) {
	unsigned int i;
	for (i = 0; i < dst_size && src[i] != until; ++i)
		dst[i] = src[i];

	return i;
}

static unsigned int get_modification_time(char const *path) {
	struct stat st;

	if (stat(path, &st) < 0)
		return 0;

	return st.st_mtime;
}

/*
 * Local HP functions
 */

// Get an internal type representation (enum) from a string
static eType hp_type_from_str(char *s) {
	static struct {
		char *str;
		unsigned int str_sz;
		eType type;
	} type_selector[] = {
		{"int", 3, TYPE_INT},
		{"float", 5, TYPE_FLOAT},
	};

	while (*s && IS_SEP(*s))
		++s;
	if (!s)
		return TYPE_UNKNOWN;

	unsigned int i;
	for (i = 0;
		i < sizeof(type_selector) / sizeof(*type_selector);
		++i)
		if (!strncmp(s, type_selector[i].str, type_selector[i].str_sz))
			return type_selector[i].type;

	return TYPE_UNKNOWN;
}

// Get an internal value representation (struct) from a string
static hp_value_t hp_value_from_str(eType type, char *s) {
	hp_value_t value;

	value.type = type;
	if (type == TYPE_INT)
		value.value.integer = atoi(s);
	else if (type == TYPE_FLOAT)
		value.value.floating_integer = (float)atof(s);
	else
		value.value.integer = 0;

	return value;
}

// Get a variable from the cache
static hp_vars_list_t *hp_get_tweakable_var(char const *path, hp_id_t id) {
	DBG_OUT("Trying to find var #%u in %s", id, path);
	hp_vars_list_t *o;
	for (o = hp_tweakable_vars; o; o = o->next) {
		DBG_OUT(":%s; %u - %s; %u",
			path, id,
			o->path, o->id);
		if (!strcmp(o->path, path)
			&& o->id == id)
			return o;
	}

	DBG_OUT("Var not found !");

	return NULL;
}

// Get a file from the cache
static hp_files_list_t *hp_get_tweakable_file(char const *path) {
	hp_files_list_t *o;
	for (o = hp_tweakable_files; o; o = o->next)
		if (!strcmp(o->path, path))
			return o;

	return NULL;
}

// Add a file to the cache
static hp_files_list_t *hp_reg_file(char const *path) {
	hp_files_list_t *new = hp_get_tweakable_file(path);

	if (new)
		return new;

	if (!(new = malloc(sizeof(hp_files_list_t))))
		return NULL;

	strncpy(new->path, path, BUF_SIZE);
	new->mod_timestamp = get_modification_time(path);

	new->next = hp_tweakable_files;
	hp_tweakable_files = new;

	DBG_OUT("Registered file: %s", path);

	return new;
}

// Add a variable to the cache
static hp_vars_list_t *hp_reg_val(char const *path, hp_id_t id, eType type, void *value) {
	hp_vars_list_t *new;

	if (!(new = malloc(sizeof(hp_vars_list_t))))
		return NULL;

	strncpy(new->path, path, BUF_SIZE);
	new->id = id;

	new->value.type = type;
	if (type == TYPE_INT)
		memcpy(&new->value.value.integer, value, sizeof(int));
	else if (type == TYPE_FLOAT)
		memcpy(&new->value.value.floating_integer, value, sizeof(float));
	else {
		DBG_OUT("Unknown type given: #%u", type);
		free(new);
		return NULL;
	}

	DBG_OUT("Registered variable of type %u (%ust)", type, id);

	new->next = hp_tweakable_vars;
	hp_tweakable_vars = new;

	return new;
}

// Parse a file, and refresh all the variables
static int hp_refresh_file(hp_files_list_t *file) {
	FILE *fin;
	char buffer[BUF_SIZE];

	if (!file) {
		DBG_OUT("Invalid pointer passed to the refresh_file function");
		return -1;
	}

	DBG_OUT("Refreshing file %s", file->path);
	unsigned int mtime = get_modification_time(file->path);
	if (mtime <= file->mod_timestamp) {
		DBG_OUT("File %s has not changed, skipping", file->path);
		return 1;
	}

	if (!(fin = fopen(file->path, "r"))) {
		DBG_OUT("Unable to open file %s", file->path);
		return 1;
	}

	unsigned int nb_lines = 0;
	unsigned int nb_macros = 0;
	while (fgets(buffer, BUF_SIZE, fin)) {
		++nb_lines;
		int idx = left_strip_index(buffer);
		if (idx < 0
			|| !strncmp(buffer + idx, "//", 2))
			continue;

		char *s = (char*)&buffer;
		char *m_idx;
		char const * const macro = "_H" "P(";
		for (m_idx = strstr(s, macro);
				m_idx;
				m_idx = strstr(s, macro)) {
			eType type;
			char s_type[TYPE_BUF_SIZE];
			unsigned int nbc_type;

			hp_value_t value;
			char s_value[VALUE_BUF_SIZE];
			unsigned int nbc_value;

			m_idx += 4;
			s = m_idx;

			char *comma = strchr(m_idx, ',');
			if (!comma) {
				DBG_OUT("No comma found on line #%u",
						nb_lines);
				continue;
			}

			nbc_type = consume_char_until(m_idx,
					(char*)&s_type,
					TYPE_BUF_SIZE,
					',');
			if (!nbc_type) {
				DBG_OUT("Missing type on line #%u",
						nb_lines);
				continue;
			}
			s_type[nbc_type] = 0;

			if (!strchr(comma, ')')) {
				DBG_OUT("Missing ) on line #%u",
						nb_lines);
				continue;
			}

			nbc_value = consume_char_until(
					comma + 1,
					(char*)&s_value,
					VALUE_BUF_SIZE,
					')');
			if (!nbc_value) {
				DBG_OUT("Missing value on line #%u",
						nb_lines);
				continue;
			}
			s_value[nbc_value] = 0;

			type = hp_type_from_str((char*)&s_type);

			if (type == TYPE_UNKNOWN) {
				DBG_OUT("Unknow type %s on line #%u",
						s_type, nb_lines);
				continue;
			}

			DBG_OUT("Detected macro: %s,%s",
					s_type, s_value);
			value = hp_value_from_str(type, (char*)&s_value);
			hp_vars_list_t *var = hp_get_tweakable_var(file->path, nb_macros);

			if (!var) {
				DBG_OUT("Unable to find the var in the cache");
				continue;
			}

			++nb_macros;
			if (var->value.type != type) {
				DBG_OUT("Type changed from %u to %u, skipping",
						var->value.type, type);
				continue;
			}
			if (memcmp(&var->value, &value, sizeof(hp_value_t))) {
				DBG_OUT("Hot patching the macro line #%u",
						nb_lines);
				memcpy(&var->value, &value, sizeof(hp_value_t));
				file->mod_timestamp = time(0);
			}
		}
	}

	fclose(fin);

	return 0;
}

/*
 * Exported functions
 */

// Return the integer associated to the given id from the cache
int *hp_get_tweakable_int(char const *path, hp_id_t id, int value) {
	hp_vars_list_t *var = hp_get_tweakable_var(path, id);

	if (!hp_reg_file(path)) {
		DBG_OUT("Unable to register file %s", path);
		return NULL;
	}

	if (!var) {
		DBG_OUT("Variable not registered, registering it");
		var = hp_reg_val(path, id, TYPE_INT, &value);
	}

	return (var ? &var->value.value.integer : NULL);
}

// Return the floatting point integer associated to the given id from the cache
float *hp_get_tweakable_float(char const *path, hp_id_t id, float value) {
	hp_vars_list_t *var = hp_get_tweakable_var(path, id);

	if (!hp_reg_file(path)) {
		DBG_OUT("Unable to register file %s", path);
		return NULL;
	}

	if (!var) {
		DBG_OUT("Variable not registered, registering it");
		var = hp_reg_val(path, id, TYPE_FLOAT, &value);
	}

	return (var ? &var->value.value.floating_integer : NULL);
}

// Parse the files in the cache, and update the cached values
unsigned int hp_refresh_tweakable_vars() {
	hp_files_list_t *file;
	unsigned int nb_errors = 0;
	for (file = hp_tweakable_files; file; file = file->next)
		if (hp_refresh_file(file))
			++nb_errors;

	return nb_errors;
}

// Parse a single file, and update the cached values inside it
int hp_refresh_tweakable_file(char const *path) {
	hp_files_list_t *file;

	if (!path)
		return -1;

	if (!(file = hp_get_tweakable_file(path)))
		return 1;

	return hp_refresh_file(file);
}

#endif
