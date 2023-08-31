#pragma once

#include "utils.h"
#include "flags.h"

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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
		create_symlink(target, path);
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
		char prompt = prompt_user("do you want to merge the two and symlink the whole directory?", "yn", 'n');
		if (prompt == 'y') {
			merge_directory(path, target);
			// `merge_directory` removes `path`
		} else {
			return;
		}
	}
	DLOG("log: creating symlink `%s` to `%s`", path, target);
	create_symlink(target, path);
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
