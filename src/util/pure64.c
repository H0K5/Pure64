/* =============================================================================
 * Pure64 -- a 64-bit OS/software loader written in Assembly for x86-64 systems
 * Copyright (C) 2008-2017 Return Infinity -- see LICENSE.TXT
 * =============================================================================
 */

#include <pure64/dir.h>
#include <pure64/file.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/** Compares an option with a command
 * line argument. This function checks
 * both short and long options.
 * @param arg The argument from the command line.
 * @param opt The long option (the two '--' are added
 * automatically).
 * @param s_opt The letter representing the short option.
 * @returns True if the argument is a match, false
 * if the argument is not a match.
 * */

static bool check_opt(const char *arg,
                      const char *opt,
                      char s_opt) {

	/* create a string version
	 * of the short option */
	char s_opt3[3];
	s_opt3[0] = '-';
	s_opt3[1] = s_opt;
	s_opt3[2] = 0;
	if (strcmp(s_opt3, arg) == 0)
		return true;

	if ((arg[0] == '-')
	 && (arg[1] == '-')
	 && (strcmp(&arg[2], opt) == 0))
		return true;

	return false;
}

/** Prints the help message.
 * @param argv0 The name of the program.
 * */

static void print_help(const char *argv0) {
	printf("Usage: %s [options] <command>\n", argv0);
	printf("\n");
	printf("Options:\n");
	printf("\t--file, -f : Specify the path to the Pure64 file.\n");
	printf("\t--help, -h : Print this help message.\n");
	printf("\n");
	printf("Commands:\n");
	printf("\tcat   : Print the contents of a file.\n");
	printf("\tcp    : Copy file from host file system to Pure64 image.\n");
	printf("\tls    : List directory contents.\n");
	printf("\tmkdir : Create a directory.\n");
	printf("\tmkfs  : Create the file system image.\n");
	printf("\trm    : Remove a file.\n");
	printf("\trmdir : Remove a directory.\n");
}

static bool is_opt(const char *argv) {
	if (argv[0] == '-')
		return true;
	else
		return false;
}

static int ramfs_export(struct pure64_dir *root, const char *filename) {

	int err;
	FILE *file;

	file = fopen(filename, "wb");
	if (file == NULL) {
		fprintf(stderr, "Failed to open '%s'.\n", filename);
		return EXIT_FAILURE;
	}

	err = pure64_dir_export(root, file);
	if (err != 0) {
		fprintf(stderr, "Failed to export RamFS file system.\n");
		fclose(file);
		return EXIT_FAILURE;
	}

	fclose(file);

	return EXIT_SUCCESS;
}

static int ramfs_import(struct pure64_dir *root, const char *filename) {

	FILE *file;

	file = fopen(filename, "rb");
	if (file == NULL ) {
		fprintf(stderr, "Failed to open '%s' for reading.\n", filename);
		return EXIT_FAILURE;
	}

	if (pure64_dir_import(root, file) != 0) {
		fprintf(stderr, "Failed to read file system from '%s'.\n", filename);
		fclose(file);
		return EXIT_FAILURE;
	}

	fclose(file);

	return EXIT_SUCCESS;
}

static int pure64_cat(struct pure64_dir *dir, int argc, const char **argv) {

	struct pure64_file *file;

	for (int i = 0; i < argc; i++) {

		file = pure64_dir_open_file(dir, argv[i]);
		if (file == NULL) {
			fprintf(stderr, "Failed to open '%s'.\n", argv[i]);
			return EXIT_FAILURE;
		}

		fwrite(file->data, 1, file->data_size, stdout);
	}

	return EXIT_SUCCESS;
}

static int pure64_cp(struct pure64_dir *root, int argc, const char **argv) {

	int err;
	struct pure64_file *dst;
	FILE *src;
	long int src_size;
	const char *dst_path;
	const char *src_path;

	if (argc <= 0) {
		fprintf(stderr, "Missing source path.\n");
		return EXIT_FAILURE;
	} else if (argc <= 1) {
		fprintf(stderr, "Missing destination path.\n");
		return EXIT_FAILURE;
	}

	src_path = argv[0];
	dst_path = argv[1];

	src = fopen(src_path, "rb");
	if (src == NULL) {
		fprintf(stderr, "Failed to open source file '%s'.\n", src_path);
		return EXIT_FAILURE;
	}

	err = 0;

	err |= fseek(src, 0L, SEEK_END);

	src_size = ftell(src);

	err |= fseek(src, 0L, SEEK_SET);

	if ((err != 0) || (src_size < 0)) {
		fprintf(stderr, "Failed to get file size of '%s'.\n", src_path);
		fclose(src);
		return EXIT_FAILURE;
	}

	err = pure64_dir_make_file(root, dst_path);
	if (err != 0) {
		fprintf(stderr, "Failed to create destination file '%s'.\n", dst_path);
		fclose(src);
		return EXIT_FAILURE;
	}

	dst = pure64_dir_open_file(root, dst_path);
	if (dst == NULL) {
		fprintf(stderr, "Failed to open destination file '%s'.\n", dst_path);
		fclose(src);
		return EXIT_FAILURE;
	}

	dst->data = malloc(src_size);
	if (dst->data == NULL) {
		fprintf(stderr, "Failed to allocate memory for destination file '%s'.\n", dst_path);
		fclose(src);
		return EXIT_FAILURE;
	}

	if (fread(dst->data, 1, src_size, src) != src_size) {
		fprintf(stderr, "Failed to read source file'%s'.\n", src_path);
		fclose(src);
		return EXIT_FAILURE;
	}

	fclose(src);

	dst->data_size = src_size;

	return EXIT_SUCCESS;
}

static int pure64_ls(struct pure64_dir *dir, int argc, const char **argv) {

	struct pure64_dir *subdir;

	if (argc == 0) {
		const char *default_args[] = { "/", NULL };
		return pure64_ls(dir, 1,  default_args);
	}

	for (int i = 0; i < argc; i++) {

		subdir = pure64_dir_open_subdir(dir, argv[i]);
		if (subdir == NULL) {
			fprintf(stderr, "Failed to open '%s'.\n", argv[i]);
			return EXIT_FAILURE;
		}

		printf("%s:\n", argv[i]);

		for (uint64_t j = 0; j < subdir->subdir_count; j++)
			printf("dir  : %s\n", subdir->subdirs[j].name);

		for (uint64_t j = 0; j < subdir->file_count; j++)
			printf("file : %s\n", subdir->files[j].name);
	}

	return EXIT_SUCCESS;
}

static int pure64_mkdir(struct pure64_dir *dir, int argc, const char **argv) {

	int err;

	for (int i = 0; i < argc; i++) {
		printf("%s\n", argv[i]);
		err = pure64_dir_make_subdir(dir, argv[i]);
		if (err != 0) {
			fprintf(stderr, "Failed to create directory '%s'.\n", argv[i]);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

static int pure64_mkfs(const char *filename, int argc, const char **argv) {

	int err;
	struct pure64_dir root;

	/* no arguments are currently
	 * needed for this command. */
	(void) argc;
	(void) argv;

	pure64_dir_init(&root);

	err = ramfs_export(&root, filename);
	if (err != EXIT_SUCCESS)
		return EXIT_FAILURE;

	pure64_dir_free(&root);

	return EXIT_SUCCESS;
}

int main(int argc, const char **argv) {

	int i;
	int err;
	const char *filename = "pure64.img";
	struct pure64_dir root;

	for (i = 1; i < argc; i++) {
		if (check_opt(argv[i], "help", 'h')) {
			print_help(argv[0]);
			return EXIT_FAILURE;
		} else if (check_opt(argv[i], "file", 'f')) {
			filename = argv[i + 1];
			i++;
		} else if (is_opt(argv[i])) {
			fprintf(stderr, "Unknown option '%s'.\n", argv[i]);
			return EXIT_FAILURE;
		} else {
			break;
		}
	}

	if (filename == NULL) {
		fprintf(stderr, "No filename specified after '--file' or '-f' option.\n");
		return EXIT_FAILURE;
	}

	if (i >= argc) {
		fprintf(stderr, "No command specified (see '--help').\n");
		return EXIT_FAILURE;
	}

	/* argv[i] should now point to a command. */

	if (strcmp(argv[i], "mkfs") == 0) {
		return pure64_mkfs(filename, i - 1, &argv[i + 1]);
	}

	pure64_dir_init(&root);

	err = ramfs_import(&root, filename);
	if (err != EXIT_SUCCESS) {
		pure64_dir_free(&root);
		return EXIT_FAILURE;
	}

	if (strcmp(argv[i], "cat") == 0) {
		err = pure64_cat(&root, argc - (i + 1), &argv[i + 1]);
	} else if (strcmp(argv[i], "cp") == 0) {
		err = pure64_cp(&root, argc - (i + 1), &argv[i + 1]);
	} else if (strcmp(argv[i], "ls") == 0) {
		err = pure64_ls(&root, argc - (i + 1), &argv[i + 1]);
	} else if (strcmp(argv[i], "mkdir") == 0) {
		err = pure64_mkdir(&root, argc - (i + 1), &argv[i + 1]);
	} else if (strcmp(argv[i], "rm") == 0) {
	} else if (strcmp(argv[i], "rmdir") == 0) {
	} else {
		fprintf(stderr, "Unknown command '%s'.\n", argv[i]);
		return EXIT_FAILURE;
	}

	err = ramfs_export(&root, filename);
	if (err != EXIT_SUCCESS) {
		pure64_dir_free(&root);
		return EXIT_FAILURE;
	}

	pure64_dir_free(&root);

	return err;
}
