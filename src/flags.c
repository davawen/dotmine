#include "flags.h"

#include <stdlib.h>
#include <string.h>

struct Flags flags;

const char *get_some_next() {
	for (; flags.i < flags.argc; flags.i++) {
		if (flags.argv[flags.i] == NULL) continue;

		return flags.argv[flags.i++]; // increment i to point to next argument
	}

	return NULL;
}

const char *get_next(const char *name) {
	const char *next = get_some_next();
	if (next == NULL) {
		ERROR("error: expected argument <%s>", name);
	}

	return next;
}

void init_flags(int argc, char **argv, const char *default_dir) {
	flags.i = 0;
	flags.argc = argc;
	flags.argv = argv;
	flags.home = getenv("HOME");

	flags.help = false;
	flags.recursive = false;

	static char buf[1024];
	int n = snprintf(buf, 1024, "%s/%s", flags.home, default_dir);
	ASSERT(n <= 1023, "error: not enough space for path");
	flags.mine = buf;
}

const char *parse_args() {
	// parse every flag
	for (flags.i = 1; flags.i < flags.argc; flags.i++) {
		// -- means end of flags
		char **curr = &flags.argv[flags.i];
		if (strcmp(*curr, "--") == 0) {
			*curr = NULL;
			break;
		}
		
		if ((*curr)[0] == '-') {
			if (strcmp(*curr, "--help") == 0 || strcmp(*curr, "-h") == 0) {
				flags.help = true;
			} else if (strcmp(*curr, "--version") == 0 || strcmp(*curr, "-v") == 0) {
				flags.version = true;
			} else if (strcmp(*curr, "--recursive") == 0 || strcmp(*curr, "-r") == 0) {
				flags.recursive = true;
			} else if (strcmp(*curr, "--mine") == 0) {
				*curr = NULL;
				curr = &flags.argv[++flags.i];
				if (flags.i >= flags.argc) ERROR("error: no value given to option --mine");
				flags.mine = *curr;
			} else {
				ERROR("error: unknown option %s", *curr);
			}

			*curr = NULL;
		}
	}

	flags.i = 1;
	return get_some_next();
}
