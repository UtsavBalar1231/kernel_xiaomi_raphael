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

#ifndef _LINUX_INIT_ASYNC_H
#define _LINUX_INIT_ASYNC_H

#include <linux/init.h>
#include <linux/completion.h>
#include <linux/slab.h>


#define _early_initcall_async(fn, subsys, level) \
	static DECLARE_COMPLETION(_##fn##done); \
	static void __init _##fn##_work(struct work_struct *w) \
	{ \
		fn(); \
		complete(&_##fn##done); \
		kfree(w); \
	} \
	static int __init _##fn##_sync(void) \
	{ \
		wait_for_completion(&_##fn##done); \
		return 0; \
	} \
	static int __init _##fn##_async(void) \
	{ \
		struct work_struct *work; \
		work = kzalloc(sizeof(struct work_struct), GFP_KERNEL); \
		if (work) { \
			INIT_WORK(work, _##fn##_work); \
			queue_work_on(WORK_CPU_UNBOUND, system_highpri_wq, \
						work); \
		} \
		return 0; \
	} \
	__define_early_initcall(_##fn##_async, subsys, level)

#define early_initcall_type_async(type, fn, subsys, level) \
	static int __init _##fn(void) \
	{ \
		if (is_early_userspace) \
			return 0; \
		return fn(); \
	} \
	type(_##fn); \
	_early_initcall_async(fn, subsys, level)

#define early_device_initcall_async(fn, subsys, level) \
	early_initcall_type_async(device_initcall, fn, subsys, level)

#define __early_initcall_async(fn, subsys, level) \
	early_device_initcall_async(fn, subsys, level)

#endif /* _LINUX_INIT_ASYNC_H */
