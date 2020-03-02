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

#ifndef _SDE_FENCE_MISR_H_
#define _SDE_FENCE_MISR_H_

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/kref.h>
#include <uapi/drm/sde_drm.h>
#include "sde_hw_mdss.h"

enum misr_fence_flag_bits {
	MISR_FENCE_FLAG_EVENT_BIT,
	MISR_FENCE_FLAG_SIGNALED_BIT,
};

/**
 * struct sde_misr_fence – structure of sde_misr_fence
 * @file:           file representing this fence
 * @refcount:       fence reference count
 * @wq:             wait queue for fence signaling
 * @roi_num:        the number of ROI misr instances per engine
 * @orig_order:     the original order of user setting rectangle
 * @updated_count:  the updated count of this fence
 * @flags:          the event flag of this fence
 * @data:           cache signature from register
 * @node:           list to associated this fence on timeline
 */
struct sde_misr_fence {
	struct file *file;
	struct kref refcount;
	wait_queue_head_t wq;
	int roi_num[ROI_MISR_MAX_MISRS_PER_CRTC];
	int orig_order[ROI_MISR_MAX_ROIS_PER_CRTC];
	int updated_count;
	unsigned long flags;
	uint32_t data[ROI_MISR_MAX_ROIS_PER_CRTC];
	struct list_head node;
};

#define MISR_ERROR(fmt, ...) pr_err("[misr error]" fmt, ##__VA_ARGS__)

/**
 * sde_misr_fence - Increments the refcount of the misr fence
 *
 * @fence: Pointer to misr fence structure
 *
 * Return: Pointer to msir fence object, or NULL
 */
static inline struct sde_misr_fence *misr_fence_get(
		struct sde_misr_fence *fence)
{
	if (fence)
		kref_get(&fence->refcount);

	return fence;
}

void misr_fence_free(struct kref *kref);
/**
 * misr_fence_put - Releases a misr fence object acquired by @misr_fence_get
 *
 * This function decrements the misr fence's reference count; the object will
 * be released if the reference count goes to zero.
 *
 * @fence: Pointer to misr fence structure
 */
static inline void misr_fence_put(struct sde_misr_fence *fence)
{
	if (fence)
		kref_put(&fence->refcount, misr_fence_free);
}

/**
 * misr_fence_create - Create a misr fence object with sde_misr_fence data
 *
 * @fence: Pointer to misr fence structure
 * @val: The file description of this fence object
 */
int misr_fence_create(struct sde_misr_fence **fence, uint64_t *val);

/**
 * misr_fence_signal - signal misr fence that data is ready
 *
 * @fence: Pointer to misr fence structure
 */
void misr_fence_signal(struct sde_misr_fence *fence);

/**
 * add_fence_object - add a fence object to the list
 *
 * @new_node: the new node to be added
 * @head: list head to add it
 */
static inline void add_fence_object(struct list_head *new_node,
		struct list_head *head)
{
	struct sde_misr_fence *fence =
			container_of(new_node, struct sde_misr_fence, node);

	misr_fence_get(fence);
	list_add_tail(new_node, head);
}

/**
 * add_fence_object - delete a fence object of the list
 *
 * @node: the fence which should be deleted from the list.
 */
static inline void del_fence_object(struct sde_misr_fence *fence)
{
	struct list_head *node = &fence->node;

	list_del_init(node);
	misr_fence_put(fence);
}

/**
 * get_fence_instance - get the first fence instance from the list
 *
 * @head: the head of the list.
 */
static inline struct sde_misr_fence *get_fence_instance(
		struct list_head *head)
{
	if (list_empty(head))
		return NULL;

	return list_first_entry_or_null(head, struct sde_misr_fence, node);
}

#endif /* _SDE_FENCE_MISR_H_ */

