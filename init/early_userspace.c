/* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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


#include <linux/init.h>
#include <linux/completion.h>
#include <linux/async.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/kallsyms.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ftrace.h>

struct subsystem_work {
	struct work_struct work;
	int id;
};

static DECLARE_COMPLETION(populate_done);
static DECLARE_COMPLETION(subsys_done);
static DECLARE_COMPLETION(plat_subsys_done);
static atomic_t subsys_finish = ATOMIC_INIT(EARLY_SUBSYS_NUM);
bool is_early_userspace;
EXPORT_SYMBOL(is_early_userspace);

static int __init early_userspace(char *p)
{
	is_early_userspace = true;
	return 0;
}
early_param("early_userspace", early_userspace);

static initcall_t *early_initcall_levels[] = {
	__early0_initcall_start,
	__early1_initcall_start,
	__early2_initcall_start,
	__early3_initcall_start,
	__early4_initcall_start,
	__early5_initcall_start,
	__early6_initcall_start,
	__early7_initcall_start,
	__early_initcall_end,
};

static void do_early_subsys_init(int id)
{
	int ret;
	initcall_t *fn;

	for (fn = early_initcall_levels[id]; fn < early_initcall_levels[id+1];
		fn++) {
		ret = (*fn)();
		if (ret) {
			print_ip_sym(*((unsigned long *)fn));
			pr_err("fails with %d\n", ret);
		}
	}
}

void __ref early_subsys_finish(void)
{
	if (atomic_dec_and_test(&subsys_finish)) {
		ftrace_free_init_mem();
		free_initmem();
	}
}

static void early_subsys_init(struct work_struct *w)
{
	struct subsystem_work *subsys_work =
		container_of(w, struct subsystem_work, work);

	do_early_subsys_init(subsys_work->id);
	early_subsys_finish();
	kfree(subsys_work);
}

static void early_system_init(struct work_struct *w)
{
	struct subsystem_work *subsys_work;
	int id;

	do_early_subsys_init(EARLY_SUBSYS_PLATFORM);
	complete(&plat_subsys_done);
	pr_info("early_common_platform initialized\n");
	wait_for_completion(&populate_done);
	pr_info("early_subsystems starting\n");

	for (id = EARLY_SUBSYS_1; id < EARLY_SUBSYS_NUM; id++) {
		subsys_work = kzalloc(sizeof(struct subsystem_work),
			GFP_KERNEL);
		if (subsys_work) {
			subsys_work->id = id;
			INIT_WORK(&subsys_work->work, early_subsys_init);
			queue_work_on(WORK_CPU_UNBOUND, system_unbound_wq,
				&subsys_work->work);
		} else {
			pr_err("no mem to start early_subsys_init\n");
		}
	}
	kfree(w);
}

static const struct of_device_id early_devices_match_table[] = {
	{ .compatible = "qcom,early-devices" },
	{ }
};
MODULE_DEVICE_TABLE(of, early_devices_match_table);

static int early_devices_probe(struct platform_device *pdev)
{
	struct device_node *early_node;
	int i, len = 0;
	struct work_struct *work;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	of_find_property(np, "devices", &len);
	for (i = 0; i < len / sizeof(u32); i++) {
		early_node = of_parse_phandle(np, "devices", i);
		of_platform_node_probe(early_node, pdev->dev.parent);
		of_node_put(early_node);
	}
	work = kzalloc(sizeof(struct work_struct), GFP_KERNEL);
	if (work) {
		INIT_WORK(work, early_system_init);
		queue_work_on(WORK_CPU_UNBOUND, system_unbound_wq, work);
	} else {
		pr_err("no mem to start early_system_init\n");
	}

	return 0;
}

static struct platform_driver early_devices_driver = {
	.probe		= early_devices_probe,
	.driver		= {
		.name	= "early-devices",
		.of_match_table = early_devices_match_table,
	},
};

static int __init early_devices_init(void)
{
	if (is_early_userspace)
		return platform_driver_register(&early_devices_driver);
	else
		return 0;
}
arch_initcall(early_devices_init);

static void __exit early_devices_exit(void)
{
	if (is_early_userspace)
		platform_driver_unregister(&early_devices_driver);
}
module_exit(early_devices_exit);

static int __init early_populate_sync(void)
{
	complete(&populate_done);
	pr_info("early populate_sync start\n");
	if (is_early_userspace)
		wait_for_completion(&plat_subsys_done);
	pr_info("early populate_sync end\n");
	return 0;
}
subsys_initcall(early_populate_sync);

static int __init early_subsys_sync(void)
{
	complete(&subsys_done);
	return 0;
}
subsys_initcall_sync(early_subsys_sync);

static int __init early_subsys_wait(void)
{
	pr_info("early subsys wait start\n");
	wait_for_completion(&subsys_done);
	pr_info("early subsys wait end\n");
	return 0;
}
early_init(early_subsys_wait, EARLY_SUBSYS_1, EARLY_INIT_LEVEL7);
