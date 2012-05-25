hot_patch
=========

Introduction
------------

The hot_patch module (hot_patch.c + hot_patch.h) is a small and straight forward
implementation of a very useful concept: the ability to edit certain constants
in your code, and to make your (running) application immediately take them in
account without having to recompile said program.

How it works
----------------

- Pass the values of the constants you want to be able to have live access to to
  the _HP macro
- Compile your application in debug mode with the hot_patch module (the module
  considers the application is in debug mode if the DEBUG macro is defined)
- Run your application in a separate terminal/window
- Edit the content of the _HP macros in your source code, save the file
- Profit !

Example
-------

Compile the following code with the hot_patch module (don't forget to define the
DEBUG macro, with the -D option for example).
```c
int main() {
	int editable = _HP(int, 1);
	while (editable) {
		fprintf(stdout, "Editable: %d\n", editable);
		sleep(3);
	}

	return 0;
}
```
Run the resulting binary in a separate window , and edit the '1' in the _HP
macro by.. something else, and watch your app display "Editable:
SOMETHING_ELSE".

Note that the first field of the macro is mandatory, as it represents the type
of the editable variable. Consequently, modifying it at runtime will result in
an undefined behavior.

Fields of use
-------------

Although this module is kind of a PoC (Proof of Concept), I find it really handy to have when I
have to code applications that use "constants" I don't really know the value
they should take when I'm developing the application.
E.g: for debugging purpose, I like to change the gravity of my game engines at
runtime: it spares me the parsing of a configuration file/the recompilation of
the whole game.
It could also be used to change the maximum number of FPS your game runs at, or
the maximum number of entities that are to be updated every second.
