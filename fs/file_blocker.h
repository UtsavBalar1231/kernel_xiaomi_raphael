/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2019-2020 Yaroslav Furman <yaro330@gmail.com>.
 *
 * Header for file blocker's code.
 */
#include <linux/kernel.h>
#include <linux/string.h>

#ifdef CONFIG_BLOCK_UNWANTED_FILES
static char *files_array[] = {
	"fde", "lspeed", "nfsinjector", "lkt"
};

static char *paths_array[] = {
	"/data/adb/modules"
};

static bool inline check_file(const char *name)
{
	int i, f;

	for (f = 0; f < ARRAY_SIZE(paths_array); ++f) {
		const char *path_to_check = paths_array[f];

		if (!strncmp(name, path_to_check, strlen(path_to_check))) {
			for (i = 0; i < ARRAY_SIZE(files_array); ++i) {
				const char *filename = name + strlen(path_to_check) + 1;
				const char *filename_to_check = files_array[i];

				/* Leave only the actual filename for strstr check */
				if (strstr(filename, filename_to_check)) {
					pr_info("%s: blocking %s\n", __func__, filename);
					return 1;
				}
			}
		}
	}
	return 0;
}
#else
static bool inline check_file(const char *name)
{
	return 0;
}
#endif
