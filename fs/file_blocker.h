/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2019-2020 Yaroslav Furman <yaro330@gmail.com>.
 *
 * Header for file blocker's code.
 */
#include <linux/kernel.h>
#include <linux/string.h>

#ifdef CONFIG_BLOCK_UNWANTED_FILES
#define BLOCKED_FILES "fde", "lspeed", "nfsinjector", "lkt"
#define BLOCKED_PATHS "/data/adb/modules"
static char *files[] = {
	BLOCKED_FILES
};

static char *paths[] = {
	BLOCKED_PATHS
};

static bool inline check_file(const char *name)
{
	int i, f;
	for (f = 0; f < ARRAY_SIZE(paths); ++f) {
		if (!strncmp(name, paths[f], strlen(paths[f]))) {
			for (i = 0; i < ARRAY_SIZE(files); ++i) {
				const char *actual_name = name + strlen(paths[f]) + 1;
				/* Leave only the actual filename for strstr check */
				if (strstr(actual_name, files[i])) {
					pr_info("blocking %s\n", actual_name);
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
