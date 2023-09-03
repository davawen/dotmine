#pragma once

#include <utils.h>

#include <stdbool.h>

extern struct Flags {
	/// the current argument
	int i;
	int argc;
	char **argv;
	const char *home;

	bool help;
	bool version;
	bool recursive;
	const char *mine;
} flags;

/// May return NULL if there is no next argument
const char *get_some_next();

/// panics on missing argument.
/// returns a non null pointer.
const char *get_next(const char *name);

/// inits the global flags variable.
/// `default_dir` is the default mine directory (without any home prefix) 
void init_flags(int argc, char **argv, const char *default_dir);

/// returns the chosen subcommand or NULL if there wasn't any
const char *parse_args();
