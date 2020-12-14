/* Copyright (c) 2017-2019, The Linux Foundataion. All rights reserved.
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

#include <linux/io.h>
#include <linux/module.h>

#include "cam_debug_util.h"

static uint debug_mdl;
module_param(debug_mdl, uint, 0644);

const char *cam_get_module_name(unsigned int module_id)
{
	return NULL;
}

void cam_debug_log(unsigned int module_id, const char *func, const int line,
	const char *fmt, ...)
{
}
