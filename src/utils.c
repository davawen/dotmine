#include "utils.h"

#include <ctype.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>

#include "flags.h"

bool strstartswith(const char *s, const char *prefix) {
	while (*s != '\0' && *prefix != '\0') {
		if (*s != *prefix) return false;

		s++;
		prefix++;
	}
	return *prefix == '\0';
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
		free(path_dir);
	} else {
		strcpy(link_copy, buf);
	}

	ASSERT(realpath(link_copy, buf) != NULL, "error: not enough space reserved for link (errno = %i)", errno);
}

char *get_target_path(const char *path) {
	size_t home_length = strlen(flags.home);
	ASSERT(home_length > 0, "error: invalid HOME variable");

	ASSERT(strstartswith(path, flags.home), "error: file/directory `%s` is not in home directory", path);
	ASSERT(!strstartswith(path, flags.mine), "error: file/directory `%s` is already in mine(`%s`)", path, flags.mine);

	char *output = malloc(1024);
	ASSERT(output != NULL, "error: malloc failed with errno = %i (buy more ram lol)", errno);

	const char *separator = flags.home[home_length-1] == '/' ? "/" : "";

	int o = snprintf(output, 1024, "%s%s%s", flags.mine, separator, path + home_length); //skip home dir
	ASSERT(o <= 1023, "error: merged path is too long");

	return output;
}


void create_symlink(const char *target, const char *link_name) {
	int len = strlen(link_name);

	bool free_link = false;
	if (link_name[len - 1] == '/') { // remove trailing slashes
		char *link = strdup(link_name);
		free_link = true;

		int i = len - 1;
		while (link[i] == '/') i--;

		if (i >= 0) link[i+1] = '\0';

		link_name = link;
	}

	ASSERT(symlink(target, link_name) == 0, "error: symlink failed with errno = %i", errno);

	if (free_link) free((void *)link_name);
}

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

