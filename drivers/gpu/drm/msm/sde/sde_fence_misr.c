/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

#include <linux/export.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/anon_inodes.h>
#include "sde_fence_misr.h"

static ssize_t misr_fence_read(struct file *file, char __user *user_buf,
		size_t count, loff_t *pos)
{
	struct sde_misr_fence *fence = file->private_data;
	int len = 0;
	int i;

	for (i = 0; i < ROI_MISR_MAX_MISRS_PER_CRTC; ++i)
		len += fence->roi_num[i];

	len *= sizeof(uint32_t);
	len = min_t(size_t, count, len);
	if (copy_to_user(user_buf, &fence->data, len)) {
		MISR_ERROR("failed to copy_to_user()\n");
		return -EFAULT;
	}

	return len;
}

static unsigned int misr_fence_poll(struct file *file, poll_table *wait)
{
	struct sde_misr_fence *fence = file->private_data;
	int ret = 0;

	poll_wait(file, &fence->wq, wait);

	if (test_and_clear_bit(MISR_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		ret = POLLIN;

	return ret;
}

void misr_fence_free(struct kref *kref)
{
	struct sde_misr_fence *fence =
		container_of(kref, struct sde_misr_fence, refcount);

	kfree(fence);
}

static int misr_fence_release(struct inode *inode, struct file *file)
{
	struct sde_misr_fence *fence = file->private_data;

	misr_fence_put(fence);

	return 0;
}

static const struct file_operations misr_fence_fops = {
	.release = misr_fence_release,
	.poll = misr_fence_poll,
	.read = misr_fence_read,
};

int misr_fence_create(struct sde_misr_fence **fence, uint64_t *val)
{
	struct sde_misr_fence *misr_fence;
	signed int fd = -EINVAL;

	fd = get_unused_fd_flags(0);
	if (fd < 0) {
		MISR_ERROR("failed to get_unused_fd_flags()\n");
		return fd;
	}

	misr_fence = kzalloc(sizeof(struct sde_misr_fence), GFP_KERNEL);
	if (!misr_fence) {
		MISR_ERROR("failed to alloc misr fence data\n");
		put_unused_fd(fd);
		return -ENOMEM;
	}

	misr_fence->file = anon_inode_getfile("misr_fence_file",
				&misr_fence_fops, misr_fence, 0);
	if (IS_ERR(misr_fence->file)) {
		MISR_ERROR("failed to anon_inode_getfile()\n");
		goto err;
	}

	init_waitqueue_head(&misr_fence->wq);
	kref_init(&misr_fence->refcount);
	INIT_LIST_HEAD(&misr_fence->node);

	fd_install(fd, misr_fence->file);
	*fence = misr_fence;
	*val = fd;

	return 0;

err:
	put_unused_fd(fd);
	kfree(misr_fence);
	return -EINVAL;
}
EXPORT_SYMBOL(misr_fence_create);

void misr_fence_signal(struct sde_misr_fence *fence)
{
	set_bit(MISR_FENCE_FLAG_SIGNALED_BIT, &fence->flags);
	wake_up_all(&fence->wq);
}
EXPORT_SYMBOL(misr_fence_signal);

