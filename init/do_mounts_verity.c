/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device-mapper.h>
#include <linux/fs.h>
#include <linux/string.h>
#include "uapi/linux/dm-ioctl.h"
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include "do_mounts.h"

#define DM_BUF_SIZE 4096

#define DM_MSG_PREFIX "verity"

#define VERITY_COMMANDLINE_PARAM_LENGTH 32
#define VERITY_ROOT_HASH_PARAM_LENGTH   65
#define VERITY_SALT_PARAM_LENGTH       65

static char dm_name[VERITY_COMMANDLINE_PARAM_LENGTH];
static char dm_version[VERITY_COMMANDLINE_PARAM_LENGTH];
static char dm_data_device[VERITY_COMMANDLINE_PARAM_LENGTH];
static char dm_hash_device[VERITY_COMMANDLINE_PARAM_LENGTH];
static char dm_data_block_size[VERITY_COMMANDLINE_PARAM_LENGTH];
static char dm_hash_block_size[VERITY_COMMANDLINE_PARAM_LENGTH];
static char dm_number_of_data_blocks[VERITY_COMMANDLINE_PARAM_LENGTH];
static char dm_hash_start_block[VERITY_COMMANDLINE_PARAM_LENGTH];
static char dm_algorithm[VERITY_COMMANDLINE_PARAM_LENGTH];
static char dm_digest[VERITY_ROOT_HASH_PARAM_LENGTH];
static char dm_salt[VERITY_SALT_PARAM_LENGTH];
static char dm_opt[VERITY_COMMANDLINE_PARAM_LENGTH];

static void __init init_param(struct dm_ioctl *param, const char *name)
{
	memset(param, 0, DM_BUF_SIZE);
	param->data_size = DM_BUF_SIZE;
	param->data_start = sizeof(struct dm_ioctl);
	param->version[0] = 4;
	param->version[1] = 0;
	param->version[2] = 0;
	param->flags = DM_READONLY_FLAG;
	strlcpy(param->name, name, sizeof(param->name));
}

static int __init dm_name_param(char *line)
{
	strlcpy(dm_name, line, sizeof(dm_name));
	return 1;
}
__setup("dmname=", dm_name_param);

static int __init dm_version_param(char *line)
{
	strlcpy(dm_version, line, sizeof(dm_version));
	return 1;
}
__setup("version=", dm_version_param);

static int __init dm_data_device_param(char *line)
{
	strlcpy(dm_data_device, line, sizeof(dm_data_device));
	return 1;
}
__setup("data_device=", dm_data_device_param);

static int __init dm_hash_device_param(char *line)
{
	strlcpy(dm_hash_device, line, sizeof(dm_hash_device));
	return 1;
}
__setup("hash_device=", dm_hash_device_param);

static int __init dm_data_block_size_param(char *line)
{
	strlcpy(dm_data_block_size, line, sizeof(dm_data_block_size));
	return 1;
}
__setup("data_block_size=", dm_data_block_size_param);

static int __init dm_hash_block_size_param(char *line)
{
	strlcpy(dm_hash_block_size, line, sizeof(dm_hash_block_size));
	return 1;
}
__setup("hash_block_size=", dm_hash_block_size_param);

static int __init dm_number_of_data_blocks_param(char *line)
{
	strlcpy(dm_number_of_data_blocks, line, sizeof(dm_number_of_data_blocks));
	return 1;
}
__setup("number_of_data_blocks=", dm_number_of_data_blocks_param);

static int __init dm_hash_start_block_param(char *line)
{
	strlcpy(dm_hash_start_block, line, sizeof(dm_hash_start_block));
	return 1;
}
__setup("hash_start_block=", dm_hash_start_block_param);

static int __init dm_algorithm_param(char *line)
{
	strlcpy(dm_algorithm, line, sizeof(dm_algorithm));
	return 1;
}
__setup("algorithm=", dm_algorithm_param);

static int __init dm_digest_param(char *line)
{
	strlcpy(dm_digest, line, sizeof(dm_digest));
	return 1;
}
__setup("digest=", dm_digest_param);

static int __init dm_salt_param(char *line)
{
	strlcpy(dm_salt, line, sizeof(dm_salt));
	return 1;
}
__setup("salt=", dm_salt_param);

static int __init dm_opt_param(char *line)
{
	strlcpy(dm_opt, line, sizeof(dm_opt));
	return 1;
}
__setup("opt=", dm_opt_param);

static void __init dm_setup_drive(void)
{
	const char *name;
	const char *version;
	const char *data_device;
	const char *hash_device;
	const char *data_block_size;
	const char *hash_block_size;
	const char *number_of_data_blocks;
	const char *hash_start_block;
	const char *algorithm;
	const char *digest;
	const char *salt;
	const char *opt;
	unsigned long long data_blocks;
	char dummy;
	char *verity_params;
	size_t bufsize;
	char *buffer = kzalloc(DM_BUF_SIZE, GFP_KERNEL);
	struct dm_ioctl *param = (struct dm_ioctl *) buffer;
	size_t dm_sz = sizeof(struct dm_ioctl);
	struct dm_target_spec *tgt = (struct dm_target_spec *) &buffer[dm_sz];

	if (!buffer)
		goto fail;
	name = dm_name;
	if (name == NULL)
		goto fail;
	DMDEBUG("(I) name=%s", name);

	if (strcmp(name, "disabled") == 0) {
		pr_info("dm: dm-verity is disabled.");
		kfree(buffer);
		return;
	}

	version = dm_version;
	if (version == NULL)
		goto fail;
	DMDEBUG("(I) version=%s", version);

	data_device = dm_data_device;
	if (data_device == NULL)
		goto fail;
	DMDEBUG("(I) data_device=%s", data_device);

	hash_device = dm_hash_device;
	if (hash_device == NULL)
		goto fail;
	DMDEBUG("(I) hash_device=%s", hash_device);

	data_block_size = dm_data_block_size;
	if (data_block_size == NULL)
		goto fail;
	DMDEBUG("(I) data_block_size=%s", data_block_size);

	hash_block_size = dm_hash_block_size;
	if (hash_block_size == NULL)
		goto fail;
	DMDEBUG("(I) hash_block_size=%s", hash_block_size);

	number_of_data_blocks = dm_number_of_data_blocks;
	if (number_of_data_blocks == NULL)
		goto fail;
	DMDEBUG("(I) number_of_data_blocks=%s", number_of_data_blocks);

	hash_start_block = dm_hash_start_block;
	if (hash_start_block == NULL)
		goto fail;
	DMDEBUG("(I) hash_start_block=%s", hash_start_block);

	algorithm = dm_algorithm;
	if (algorithm == NULL)
		goto fail;
	DMDEBUG("(I) algorithm=%s", algorithm);

	digest = dm_digest;
	if (digest == NULL)
		goto fail;
	DMDEBUG("(I) digest=%s", digest);

	salt = dm_salt;
	if (salt == NULL)
		goto fail;
	DMDEBUG("(I) salt=%s", salt);

	opt = dm_opt;
	if (opt == NULL)
		goto fail;
	DMDEBUG("(I) opt=%s", opt);

	init_param(param, name);
	if (dm_ioctrl(DM_DEV_CREATE_CMD, param)) {
		DMERR("(E) failed to create the device");
		goto fail;
	}

	init_param(param, name);
	param->target_count = 1;
	/* set tgt arguments */
	tgt->status = 0;
	tgt->sector_start = 0;
	if (sscanf(number_of_data_blocks, "%llu%c", &data_blocks, &dummy) != 1) {
		DMERR("(E) invalid number of data blocks");
		goto fail;
	}

	tgt->length = data_blocks*4096/512; /* size in sector(512b) of data dev */
	strlcpy(tgt->target_type, "verity", sizeof(tgt->target_type));
	/* build the verity params here */
	verity_params = buffer + sizeof(struct dm_ioctl) + sizeof(struct dm_target_spec);
	bufsize = DM_BUF_SIZE - (verity_params - buffer);

	verity_params += snprintf(verity_params, bufsize, "%s %s %s %s %s %s %s %s %s %s 1 %s",
							  version,
							  data_device, hash_device,
							  data_block_size, hash_block_size,
							  number_of_data_blocks, hash_start_block,
							  algorithm, digest, salt, opt);

	tgt->next = verity_params - buffer;
	if (dm_ioctrl(DM_TABLE_LOAD_CMD, param)) {
		DMERR("(E) failed to load the device");
		goto fail;
	}

	init_param(param, name);
	if (dm_ioctrl(DM_DEV_SUSPEND_CMD, param)) {
		DMERR("(E) failed to suspend the device");
		goto fail;
	}

	pr_info("dm: dm-0 (%s) is ready", data_device);
	kfree(buffer);
	return;

fail:
	pr_info("dm: starting dm-0 failed");
	kfree(buffer);
	return;

}

void __init dm_verity_setup(void)
{
	pr_info("dm: attempting early device configuration.");
	dm_setup_drive();
}
