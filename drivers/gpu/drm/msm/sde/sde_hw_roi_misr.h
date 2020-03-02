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

#ifndef _SDE_HW_ROI_MISR_H
#define _SDE_HW_ROI_MISR_H

#include "sde_hw_blk.h"
#include "sde_hw_mdss.h"

struct sde_hw_roi_misr;

/**
 * struct sde_hw_roi_misr_ops - interface to the roi_misr hardware
 *                              driver functions
 * Caller must call the init function to get the roi_misr context
 * for each roi_misr. Assumption is these functions will be called
 * after clocks are enabled
 */
struct sde_hw_roi_misr_ops {
	/**
	 * setup_roi_misr - setup roi_misr ROI info and enable misr engine.
	 * This ROI info include position, size and expected value
	 * @ctx: Pointer to roi_misr context
	 * @cfg: Pointer to roi misr configuration
	 */
	void (*setup_roi_misr)(struct sde_hw_roi_misr *ctx,
			struct sde_roi_misr_hw_cfg *cfg);

	/**
	 * reset_roi_misr - reset roi_misr register
	 * @ctx: Pointer to roi_misr context
	 */
	void (*reset_roi_misr)(struct sde_hw_roi_misr *ctx);

	/**
	 * collect_roi_misr_signature - read roi_misr signature from register,
	 * then store it into buffer
	 * @ctx: Pointer to roi_misr context
	 * @roi_idx: which ROI should be read from this MISR
	 * @misr_value: store misr value
	 * Return: signature has been read or not
	 */
	bool (*collect_roi_misr_signature)(struct sde_hw_roi_misr *ctx,
			int roi_idx, u32 *misr_value);
};

/**
 * struct sde_hw_roi_misr - roi_misr description
 * @base: Hardware block base structure
 * @hw: Block hardware details
 * @hw_top: Block hardware top details
 * @idx: ROI_MISR index
 * @caps: Pointer to roi_misr_cfg
 * @ops: Pointer to operations possible for this ROI_MISR
 * @spin_lock: Spinlock to protect setup/collect operation
 */
struct sde_hw_roi_misr {
	struct sde_hw_blk base;
	struct sde_hw_blk_reg_map hw;

	/* roi_misr */
	enum sde_roi_misr idx;
	const struct sde_roi_misr_cfg *caps;

	/* Ops */
	struct sde_hw_roi_misr_ops ops;

	spinlock_t spin_lock;
};

/**
 * sde_hw_roi_misr - convert base object sde_hw_base to container
 * @hw: Pointer to base hardware block
 * return: Pointer to hardware block container
 */
static inline struct sde_hw_roi_misr *to_sde_hw_roi_misr(struct sde_hw_blk *hw)
{
	return container_of(hw, struct sde_hw_roi_misr, base);
}

/**
 * sde_hw_roi_misr_init - initializes the roi_misr hw driver object.
 * should be called once before accessing every roi_misr.
 * @idx:  ROI_MISR index for which driver object is required
 * @addr: Mapped register io address of MDP
 * @Return: pointer to structure or ERR_PTR
 */
struct sde_hw_roi_misr *sde_hw_roi_misr_init(enum sde_roi_misr idx,
			void __iomem *addr,
			struct sde_mdss_cfg *m);

/**
 * sde_hw_roi_misr_destroy(): Destroys ROI_MISR driver context
 * @roi_misr:   Pointer to ROI_MISR driver context
 */
void sde_hw_roi_misr_destroy(struct sde_hw_roi_misr *roi_misr);

#endif /*_SDE_HW_ROI_MISR_H */

