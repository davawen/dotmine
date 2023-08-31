#pragma once

#include <stdio.h>
#include <stdbool.h>

#define LOG(format, ...) do { \
		fprintf(stderr, "%s:%i: ", __FILE_NAME__, __LINE__); \
		fprintf(stderr, format,##__VA_ARGS__); \
		fprintf(stderr, "\n"); \
	} while(0)

#define ERROR(format, ...) do { \
		LOG(format,##__VA_ARGS__); \
		exit(-1); \
	} while(0)

#define ASSERT(condition, format, ...) do { \
		if (!(condition)) { \
			fprintf(stderr, "%s:%i: ASSERTION FAILED: `" #condition "`\n  ", __FILE_NAME__, __LINE__); \
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



bool strstartswith(const char *s, const char *prefix);
void get_link_path(const char *link, char *buf, ssize_t bufsize);

/// returns the given path in mine directory
/// returns an allocated buffer that needs to be free'd
char *get_target_path(const char *path);

/// removes trailing `/`s from `link_name`
/// panics on any other `symlink` error
void create_symlink(const char *target, const char *link_name);

/// Prompts the user with the given choices and returns which index was chosen.
/// Choices is a list of possible one character string the user can enter.
/// They should only be ASCII lowercase alphabetical characters.
/// Use '\0' to not pick a default value
/// Returns the chosen char
char prompt_user(const char *prompt, const char *choices, char default_value);

/// panics if given path doesn't exist
/// panics on filesystem error
void remove_recursive(const char *path);
/// Creates a directory at the given path.
/// Panics on `mkdir` error.
/// Asks the user to overwrite if file already exists.
/// Does nothing if a directory already exists.
void create_directory(const char *path);
/// Create the directory structure up to the last file/directory.
/// Basically the same as `mkdir -p $(dirname <path>)`.
/// Takes a char *, as it modifies its input, but every modification is reversed by the end of the function
void create_structure(char *path);
