/*
 * xor.c : Multiple Devices driver for Linux
 *
 * Copyright (C) 1996, 1997, 1998, 1999, 2000,
 * Ingo Molnar, Matti Aarnio, Jakub Jelinek, Richard Henderson.
 *
 * Dispatch optimized RAID-5 checksumming functions.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example /usr/src/linux/COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define BH_TRACE 0
#include <linux/module.h>
#include <linux/gfp.h>
#include <linux/raid/xor.h>
#include <linux/jiffies.h>
#include <linux/preempt.h>
#include <asm/xor.h>

#ifndef XOR_SELECT_TEMPLATE
#define XOR_SELECT_TEMPLATE(x) (x)
#endif

/* The xor routines to use.  */
static struct xor_block_template *active_template;

void
xor_blocks(unsigned int src_count, unsigned int bytes, void *dest, void **srcs)
{
	unsigned long *p1, *p2, *p3, *p4;

	p1 = (unsigned long *) srcs[0];
	if (src_count == 1) {
		active_template->do_2(bytes, dest, p1);
		return;
	}

	p2 = (unsigned long *) srcs[1];
	if (src_count == 2) {
		active_template->do_3(bytes, dest, p1, p2);
		return;
	}

	p3 = (unsigned long *) srcs[2];
	if (src_count == 3) {
		active_template->do_4(bytes, dest, p1, p2, p3);
		return;
	}

	p4 = (unsigned long *) srcs[3];
	active_template->do_5(bytes, dest, p1, p2, p3, p4);
}
EXPORT_SYMBOL(xor_blocks);

/* Set of all registered templates.  */
static struct xor_block_template *__initdata template_list;

#define BENCH_SIZE (PAGE_SIZE)

static void __init
do_xor_speed(struct xor_block_template *tmpl, void *b1, void *b2)
{
	int speed;
	unsigned long now, j;
	int i, count, max;

	tmpl->next = template_list;
	template_list = tmpl;

	preempt_disable();

	/*
	 * Count the number of XORs done during a whole jiffy, and use
	 * this to calculate the speed of checksumming.  We use a 2-page
	 * allocation to have guaranteed color L1-cache layout.
	 */
	max = 0;
	for (i = 0; i < 5; i++) {
		j = jiffies;
		count = 0;
		while ((now = jiffies) == j)
			cpu_relax();
		while (time_before(jiffies, now + 1)) {
			mb(); /* prevent loop optimzation */
			tmpl->do_2(BENCH_SIZE, b1, b2);
			mb();
			count++;
			mb();
		}
		if (count > max)
			max = count;
	}

	preempt_enable();

	speed = max * (HZ * BENCH_SIZE / 1024);
	tmpl->speed = speed;

	printk(KERN_INFO "   %-10s: %5d.%03d MB/sec\n", tmpl->name,
	       speed / 1000, speed % 1000);
}

static int __init
calibrate_xor_blocks(void)
{
	template_list = &xor_block_32regs_p;
	xor_block_32regs_p.next = &xor_block_32regs;
	xor_block_32regs.next = &xor_block_8regs_p;
	xor_block_8regs_p.next = &xor_block_8regs;
	xor_block_8regs.next = NULL;
	active_template = &xor_block_8regs;
	return 0;
}

static __exit void xor_exit(void) { }

MODULE_LICENSE("GPL");

/* when built-in xor.o must initialize before drivers/md/md.o */
core_initcall(calibrate_xor_blocks);
module_exit(xor_exit);
