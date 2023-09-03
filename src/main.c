#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>
#include <dirent.h>

#include "utils.h"
#include "flags.h"

#include "add.h"

// TODO: (PROJECT WIDE) Dynamic memory allocation for computed paths

void command_add() {
	if (flags.help) {
		printf("usage: " NAME " [--mine MINE] [-r|--recursive] add <path>\n\n");
		printf("Adds a file or a directory to the mine\n");
		return;
	}

	// move `path` to ~/dotmine/`path`
	// symlink `path` to ~/dotmine/`path`
	// done!

	const char *path = get_next("path");

	if (access(path, F_OK) != 0) {
		ERROR("error: input path `%s` doesn't exist", path);
	}

	char *target = get_target_path(path);

	DLOG("log: creating structure for `%s`", target);
	create_structure(target);

	DLOG("log: moving `%s` to `%s`", path, target);
	add_path(path, target);

	free(target);

	printf("success!\n");
}

void command_show() {
	if (flags.help) {
		printf("usage: " NAME " [--mine MINE] show\n\n");
		return;
	}
}

int main(int argc, char **argv) {
	init_flags(argc, argv, "dotmine");
	const char *subcommand = parse_args();

	if (flags.version) {
		printf(NAME ": version " VERSION "\n");
		return 0;
	}

	if (subcommand != NULL) {
		#define COMMAND(name) do { \
			if (strcmp(subcommand, #name) == 0) { \
				command_##name(); \
				return 0; \
			} } while(0)

		COMMAND(add);
		COMMAND(show);

		#undef COMMAND

		if (!flags.help) {
			printf("error: unknown subcommand \"%s\"\n", subcommand);
			printf("you can get information about available subcommands using `dotmine --help`\n");
			return -1;
		}
	}

	printf("usage: " NAME " [options] <subcommand> args...\n\n");
	printf("options:\n");
	printf("  -h, --help      Show this help, or help about the given subcommand\n");
	printf("  -v, --version   Show the version\n");
	printf("  -r, --recursive Don't symlink directories, always traverse them and symlink files\n");
	printf("  --mine          Set location of dotmine folder (default: ~/dotmine)\n");
	printf("\n");
	printf("subcommands:\n");
	printf("    add <path> \n");
	printf("        Adds a file or a directory to the mine\n");
	printf("    show\n");
	printf("        Show the tree of files in the mine and where they point to\n");

	return 0;
}
