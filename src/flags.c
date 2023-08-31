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
	static char buf[1024];
	int n = snprintf(buf, 1024, "%s/%s", flags.home, default_dir);
	ASSERT(n <= 1023, "error: not enough space for path");

	flags.mine = buf;
}

const char *parse_args() {
	// parse every flag
	for (flags.i = 1; flags.i < flags.argc; flags.i++) {
		// -- means end of flags
		if (strcmp(flags.argv[flags.i], "--") == 0) {
			flags.argv[flags.i] = NULL;
			break;
		}
		
		if (flags.argv[flags.i][0] == '-') {
			if (strcmp(flags.argv[flags.i], "--help") == 0 || strcmp(flags.argv[flags.i], "-h") == 0) {
				flags.help = true;
			} else if (strcmp(flags.argv[flags.i], "--mine") == 0) {
				flags.argv[flags.i] = NULL;
				flags.i++;
				if (flags.i >= flags.argc) ERROR("error: no value given to option --mine");
				flags.mine = flags.argv[flags.i];
			} else {
				ERROR("error: unknown option %s", flags.argv[flags.i]);
			}

			flags.argv[flags.i] = NULL;
		}
	}

	flags.i = 1;
	return get_some_next();
}
