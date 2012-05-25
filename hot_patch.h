/*
 * hot_patch.h for hot_patch
 * by lenorm_f
 */

#pragma once

#ifdef DEBUG

typedef unsigned int hp_id_t;

// List of available return types
int *hp_get_tweakable_int(char const*, hp_id_t, int);
float *hp_get_tweakable_float(char const*, hp_id_t, float);

// Refreshes all the variables passed to the _HP macro, regardless
// of the file in which the call is made
// Returns the number of files the function couldn't parse
unsigned int hp_refresh_tweakable_vars();

// Refreshes all the variables passed to the _HP macro in a given file
// Returns 0 if everything went alright, another value otherwise
int hp_refresh_tweakable_file(char const*);

#define _HP(type, n) (*hp_get_tweakable_##type(__FILE__, __COUNTER__, n))
#else
#define _HP(type, n) n
#endif
