/*
 * main.c for hot_patch
 * by lenorm_f
 */

#include <stdio.h>
#include <unistd.h>
#include "hot_patch.h"

// Uncomment for debugging purposes
// #define DEBUG_HP

int main() {
	for (;;) {
		int status = _HP(int, 0);

		fprintf(stdout, "Status: %d %.3f %d %f\n", status,
			_HP(float, 42.042), _HP(int, 25), _HP(float, 31.9));

		if (!status)
			break;

		// Refresh all the files
		// hp_refresh_tweakable_vars();

		// Only refresh the current file
		hp_refresh_tweakable_file(__FILE__);

		sleep(3);
	}

	puts("Bye");

	return 0;
}
