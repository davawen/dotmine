#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>
#include <dirent.h>

// TODO: Dynamic memory allocation for computed paths
// (very importanto)
#define LOG(format, ...) do { \
		fprintf(stderr, "%s:%i: ", __FILE__, __LINE__); \
		fprintf(stderr, format,##__VA_ARGS__); \
		fprintf(stderr, "\n"); \
	} while(0)

#define ERROR(format, ...) do { \
		LOG(format,##__VA_ARGS__); \
		exit(-1); \
	} while(0)

#define ASSERT(condition, format, ...) do { \
		if (!(condition)) { \
			fprintf(stderr, "%s:%i: ASSERTION FAILED: `" #condition "`\n  ", __FILE__, __LINE__); \
			fprintf(stderr, format,##__VA_ARGS__); \
			exit(-1); \
		} \
	} while(0)

#define TODO() ERROR("TODO: %s", __FUNCTION__)

#ifdef DEBUG
	#define DLOG(format, ...) LOG(format,##__VA_ARGS__)
	#define DBG(expr, format_specifier) printf("[" #expr "] = " format_specifier ";\n", expr)
#else
	#define DLOG(...) ;
#endif

bool strstartswith(const char *s, const char *prefix) {
	while (*s != '\0' && *prefix != '\0') {
		if (*s != *prefix) return false;

		s++;
		prefix++;
	}
	return *prefix == '\0';
}

struct {
	bool help;
	const char *mine;
} flags;

/// Path to home directory (set in main)
const char *home;

const char *default_dir = "dotmine";

/// returns the given path in mine dir
/// returns an allocated buffer that needs to be free'd
char *get_target_path(const char *path) {
	size_t home_length = strlen(home);
	ASSERT(home_length > 0, "error: invalid HOME variable");

	ASSERT(strstartswith(path, home), "error: file/directory `%s` is not in home directory", path);
	ASSERT(!strstartswith(path, flags.mine), "error: file/directory `%s` is already in mine(`%s`)", path, flags.mine);

	char *output = malloc(1024);
	ASSERT(output != NULL, "error: malloc failed with errno = %i (buy more ram lol)", errno);

	const char *separator = home[home_length-1] == '/' ? "/" : "";

	int o = snprintf(output, 1024, "%s%s%s", flags.mine, separator, path + home_length); //skip home dir
	ASSERT(o <= 1023, "error: merged path is too long");

	return output;
}

void get_link_path(const char *link, char *buf, ssize_t bufsize) {
	ssize_t n = readlink(link, buf, 1024);
	ASSERT(n != -1, "error: readlink failed with errno = %i", errno);
	ASSERT(n < bufsize-1, "error: not enough space for symbolic link path"); // leave one character for null termination
	ASSERT(n > 0, "error: empty link");
	buf[n] = '\0';

	char link_copy[bufsize];

	// symbolic link path may be relative
	if (*buf != '/') {
		char *path_dir = strdup(link);
		path_dir = dirname(path_dir);

		snprintf(link_copy, bufsize, "%s/%s", path_dir, buf);
	} else {
		strcpy(link_copy, buf);
	}

	ASSERT(realpath(link_copy, buf) != NULL, "error: not enough space reserved for link (errno = %i)", errno);
}

/// May return NULL if there is no next argument
const char *get_some_next(int *i, int argc, char **argv) {
	for (; *i < argc; (*i)++) {
		if (argv[*i] == NULL) continue;

		return argv[(*i)++]; // increment i to point to next argument
	}

	return NULL;
}

/// panics on missing argument.
/// returns a non null pointer.
const char *get_next(const char *name, int *i, int argc, char **argv) {
	const char *next = get_some_next(i, argc, argv);
	if (next == NULL) {
		ERROR("error: expected argument <%s>", name);
	}

	return next;
}

/// Prompts the user with the given choices and returns which index was chosen.
/// Choices is a list of possible one character string the user can enter.
/// They should only be ASCII lowercase alphabetical characters.
/// Use '\0' to not pick a default value
/// Returns the chosen char
char prompt_user(const char *prompt, const char *choices, char default_value) {
	char *indicator = malloc(strlen(choices)*2);
	char *writer = indicator;
	for (const char *c = choices; *c != '\0'; c++) {
		ASSERT(islower(*c), "error: invalid choice passed to %s: %c", __FUNCTION__, *c);
		*(writer++) = *c == default_value ? toupper(*c) : *c;
		if (*(c+1) != '\0') *(writer++) = '/';
	}
	*writer = '\0';

	char *line = NULL;
	size_t n;
	char result = '\0';
	while (result == '\0') {
		printf("%s [%s]: ", prompt, indicator);
		fflush(stdout);

		getline(&line, &n, stdin);
		for (const char *c = choices; *c != '\0'; c++) {
			if (line[0] == *c || line[0] == toupper(*c)) result = *c;
		}
		if (line[0] == '\n') result = default_value;
	}
	free(line);
	free(indicator);

	return result;
}

/// panics if given path doesn't exist
/// panics on filesystem error
void remove_recursive(const char *path) {
	struct stat sp = { 0 };
	ASSERT(lstat(path, &sp) == 0, "error: stat failed with errno = %i", errno);

	if (S_ISDIR(sp.st_mode)) {
		DLOG("log: removing contents of directory `%s`", path);

		DIR *dp = opendir(path);
		ASSERT(dp != NULL, "error: opendir failed with errno = %i", errno);

		errno = 0;
		struct dirent *ep;
		while ((ep = readdir(dp)) != NULL) {
			if (strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0) // avoid self recursion
				continue;

			char buf[1024];
			size_t n = snprintf(buf, 1024, "%s/%s", path, ep->d_name);
			ASSERT(n <= 1023, "error: not enough space allocated for path");

			remove_recursive(buf);
		}

		ASSERT(closedir(dp) == 0, "error: closedir failed with errno = %i", errno);
	} 
	DLOG("log: removing `%s`", path);
	ASSERT(remove(path) == 0, "error: remove failed with errno = %i", errno);
}

/// Creates a directory at the given path.
/// Panics on `mkdir` error.
/// Asks the user to overwrite if file already exists.
/// Does nothing if a directory already exists.
void create_directory(const char *path) {
	DLOG("log: creating directory %s", path);
	struct stat sp = { 0 };

	if (lstat(path, &sp) != 0) { // path doesn't exist, create it
		ASSERT(errno == ENOENT, "error: stat failed with errno = %i", errno);
		errno = 0;
		ASSERT(mkdir(path, 0777) == 0, "error: mkdir failed with errno = %i", errno);
	} else { // file already exists
		if (S_ISDIR(sp.st_mode)) {
			DLOG("log: already exists");
			return; // directory is already created
		}

		printf("a file already exists at `%s`\n", path);
		bool overwrite = prompt_user("overwrite it to create directory structure?", "yn", 'n') == 'y';

		if (overwrite) {
			printf("removing file.\n");
			ASSERT(remove(path) == 0, "error: remove failed with errno = %i", errno);
			ASSERT(mkdir(path, 0777) == 0, "error: mkdir failed with errno = %i", errno);
		} else {
			ERROR("error: file `%s` already exists", path);
		}
	}
}

/// Create the directory structure up to the last file/directory.
/// Basically the same as `mkdir -p $(dirname <path>)`.
/// Takes a char *, as it modifies its input, but every modification is reversed by the end of the function
void create_structure(char *path) {
	ASSERT(path != NULL, "error: passed null pointer to %s", __FUNCTION__);
	if (*path == '\0') return;

	for (char *it = path+1; *it != '\0'; it++) {
		if (*it == '/') { // found a separator
			char *separator = it;
			while (*it == '/' && *it != '\0') it++; // go through all other separators
			if (*it == '\0') break; // was actually trailing, finish now
			// NOTE: `it` now points to the first character after the separator,
			//       which will be skiped after the loop header is ran,
			//       but we don't actually care about it so it's fine

			*separator = '\0'; // make the path end at the separator
			create_directory(path);
			*separator = '/';
		}
	}
}

void handle_regular_file(const char *path, const char *target, bool symlink_resulting) {
	bool target_exists = access(target, F_OK) == 0;

	if (!target_exists) {
		ASSERT(rename(path, target) == 0, "error: rename failed with errno = %i", errno);
	} else {
		printf("a file or a directory already exists at `%s`\n", target);
		printf("do you want to (A) overwrite it or (B) delete `%s` and replace it with a symlink to %s?", path, target);
		bool overwrite = prompt_user("", "ab", '\0') == 'a';
		if (overwrite) {
			remove_recursive(target); // in case `target` is a non-empty directory
			ASSERT(rename(path, target) == 0, "error: rename failed with errno = %i", errno);
		} else { // delete path
			ASSERT(remove(path) == 0, "error: remove failed with errno = %i", errno);
		}
	}
	if (symlink_resulting) {
		ASSERT(symlink(target, path) == 0, "error: symlink failed with errno = %i", errno);
	}
}

void merge_directory(const char *path, const char *target) {
	struct stat sd = { 0 };
	ASSERT(lstat(path, &sd) == 0, "error: stat failed with errno = %i", errno);

	struct stat target_sd = { 0 };
	bool target_exists = true;
	if (lstat(target, &target_sd) != 0) {
		if (errno == ENOENT) target_exists = false;
		else ERROR("error: stat failed with errno = %i", errno);
	}

	if ( S_ISDIR(sd.st_mode) ) {
		if (!target_exists) {
			ASSERT(rename(path, target) == 0, "error: rename failed with errno = %i", errno);
		} else if (!S_ISDIR(target_sd.st_mode)) {
			printf("a file already exists at `%s`\n", target);
			char prompt = prompt_user("do you want to overwrite it? or delete this directory and symlink to it?", "os", '\0');
			if (prompt == 'o') {
				ASSERT(remove(target) == 0, "error: remove failed with errno = %i", errno);
			} else {
				remove_recursive(path);
			}
		} else {
			DIR *dp = opendir(path);
			ASSERT(dp != NULL, "error: opendir failed with errno = %i", errno);

			errno = 0;
			int readdir_errno = 0;

			struct dirent *ep;
			while ((ep = readdir(dp)) != NULL) {
				readdir_errno = errno;
				if (strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0) { // avoid self recursion
					continue;
				}


				char new_path[1024];
				const char *separator = path[strlen(path)-1] == '/' ? "" : "/";
				size_t n = snprintf(new_path, 1024, "%s%s%s", path, separator, ep->d_name);
				ASSERT(n <= 1023, "error: not enough space allocated for path");

				char new_target[1024];
				separator = target[strlen(target)-1] == '/' ? "" : "/";
				n = snprintf(new_target, 1024, "%s%s%s", target, separator, ep->d_name);
				ASSERT(n <= 1023, "error: not enough space allocated for path");

				merge_directory(new_path, new_target);
				errno = 0;
			}
			ASSERT(readdir_errno == 0, "error: readdir failed with errno = %i", errno);

			ASSERT(closedir(dp) == 0, "error: closedir failed with errno = %i", errno);
			ASSERT(rmdir(path) == 0, "error: rmdir failed with errno = %i", errno); // dir should be empty
		}
	} else if ( S_ISLNK(sd.st_mode) ) {
		char link_path[1024];
		get_link_path(path, link_path, 1024);

		DLOG("testing `%s` to `%s`", link_path, target);

		if (strcmp(link_path, target) == 0) { // remove symlink
			ASSERT(remove(path) == 0, "error: remove failed with errno = %i", errno);
		} else if (strstartswith(link_path, flags.mine)) { // broken target
			TODO();
		} else {
			handle_regular_file(path, target, false);
		}
	} else {
		handle_regular_file(path, target, false);
	}
}

void handle_directory(const char *path, const char *target) {
	struct stat target_sd = { 0 };
	if (lstat(target, &target_sd) != 0) { // target doesn't exist
		if (errno != ENOENT) ERROR("error: stat failed with errno = %i", errno);
		
		ASSERT(rename(path, target) == 0, "error: rename failed with errno = %i", errno);
	} else if (S_ISREG(target_sd.st_mode)) {
		printf("a file already exists at `%s`\n", target);
		if (prompt_user("do you want to overwrite it with this directory?", "yn", 'n') == 'y') {
			ASSERT(remove(target) == 0, "error: remove failed with errno = %i", errno);
			ASSERT(rename(path, target) == 0, "error: rename failed with errno = %i", errno);
		} else {
			return;
		}
	} else if (S_ISDIR(target_sd.st_mode)) {
		printf("a directory already exists at `%s` (you might have forgotten --recursive)\n", target);
		char prompt = prompt_user("do you want to (O) overwrite it, merge the two and (S) symlink the whole directory or do nothing?", "osn", 'n');
		if (prompt == 'o') {
			remove_recursive(target);
			ASSERT(rename(path, target) == 0, "error: rename failed with errno = %i", errno);
		} else if (prompt == 's') {
			merge_directory(path, target);
			// `merge_directory` removes `path`
		} else {
			return;
		}
	}
	DLOG("log: creating symlink `%s` to `%s`", path, target);
	ASSERT(symlink(target, path) == 0, "error: symlink failed with errno = %i", errno);
}

void add_path(const char *path, const char *target) {
	struct stat sd = { 0 };
	if (lstat(path, &sd) != 0) {
		if (errno == ENOENT) ERROR("error: given path `%s` does not exist", path);
		else ERROR("error: stat failed with errno = %i", errno);
	}

	if (S_ISLNK(sd.st_mode)) {
		DLOG("got symlink");
		char link_path[1024];
		get_link_path(path, link_path, 1024);

		DLOG("link_path: %s, target: %s", link_path, target);

		if (strcmp(link_path, target) == 0) {
			return; // nothing to do
		} else if (strstartswith(link_path, flags.mine)) { // wrong target
			// prompt user to fix
			TODO();
		} else {
			handle_regular_file(path, target, true);
		}
	} else if (S_ISREG(sd.st_mode)) {
		DLOG("got file");
		handle_regular_file(path, target, true);
	} else if (S_ISDIR(sd.st_mode)) {
		DLOG("got directory");
		handle_directory(path, target);
	} else {
		ERROR("error: file kind not handled: %u", sd.st_mode);
	}
}

void command_add(int i, int argc, char **argv) {
	// move `path` to ~/dotmine/`path`
	// symlink `path` to ~/dotmine/`path`
	// done!

	const char *path = get_next("path", &i, argc, argv);

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

void command_show(int i, int argc, char **argv) {
	(void)i;
	(void)argc;
	(void)argv;
}

void parse_args(int argc, char **argv) {
	flags.help = false;

	// parse every flag
	for (int i = 1; i < argc; i++) {
		// -- means end of flags
		if (strcmp(argv[i], "--") == 0) {
			argv[i] = NULL;
			break;
		}
		
		if (argv[i][0] == '-') {
			if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
				flags.help = true;
			} else if (strcmp(argv[i], "--mine") == 0) {
				argv[i] = NULL;
				i++;
				if (i >= argc) ERROR("error: no value given to option --mine");
				flags.mine = argv[i];
			} else {
				ERROR("error: unknown option %s", argv[i]);
			}

			argv[i] = NULL;
		}
	}

	int i = 1;
	const char *subcommand = get_some_next(&i, argc, argv);

	if (subcommand != NULL && !flags.help) {
		#define COMMAND(name) do { \
			if (strcmp(subcommand, #name) == 0) { \
				command_##name(i, argc, argv); \
				return; \
			} } while(0)

		COMMAND(add);
		COMMAND(show);

		#undef COMMAND

		ERROR("error: unknown subcommand \"%s\"", subcommand);
	} else {
		printf(NAME ": version " VERSION "\n");
		printf("usage: " NAME " [options] <subcommand> args...\n\n");
		printf("options:\n");
		printf("  -h, --help Show this help, or help about the given subcommand\n");
		printf("  --mine     Set location of dotmine folder (default: ~/dotmine)\n");
		printf("\n");
		printf("subcommands:\n");
		printf("    add <path> \n");
		printf("        Adds a file or a directory to the mine\n");
		printf("    show\n");
		printf("        Show the tree of files in the mine and where they point to\n");
	}

}

/// expands `path` from directory `root`
/// `root` needs to be an absolute path.
/// returns an allocated buffer for the result that needs to be free'd
// char *realpath_with_root(const char *root, const char *path) {
// 	ASSERT(root != NULL && path != NULL, "error: passed NULL pointer to %s", __FUNCTION__);
//
// 	char *out = malloc(PATH_MAX);
//
// 	if (strstartswith(path, "..")) {
// 		const char *last_separator = NULL;
// 		for (const char *it = root; *it != '\0'; it++) {
// 			if (*it == '/') {
// 				const char *separator = it;
// 				while (*it == '/' && *it != '\0') it++;
// 				if (*it == '\0') break; // trailing separators
//
// 				last_separator = separator;
// 			}
// 		}
// 		ASSERT(last_separator != NULL, "error: invalid argument root, not an absolute path: `%s`", root);
//
// 		size_t n = last_separator - root;
// 		ASSERT((n + strlen(path)-1) < PATH_MAX, "error: not enough space allocated for path");
//
// 		char *end = stpncpy(out, root, n); //stpncpy returns a pointer to the end
// 		strcpy(end, path+2);
// 	} else if (strstartswith(path, ".")) {
//
// 	}
// }

int main(int argc, char **argv) {
	home = getenv("HOME");

	char buf[1024];
	snprintf(buf, 1024, "%s/%s", home, default_dir);
	flags.mine = buf;

	parse_args(argc, argv);
	
	return 0;
}
