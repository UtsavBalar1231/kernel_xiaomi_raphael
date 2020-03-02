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

#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_dbg.h"
#include "sde_kms.h"
#include "sde_hw_roi_misr.h"

/* SDE_ROI_MISR_CTL */
#define ROI_MISR_OP_MODE                            0x00
#define ROI_MISR_POSITION(i)                       (0x10 + 0x4 * (i))
#define ROI_MISR_SIZE(i)                           (0x20 + 0x4 * (i))
#define ROI_MISR_CTRL(i)                           (0x30 + 0x4 * (i))
#define ROI_MISR_CAPTURED(i)                       (0x40 + 0x4 * (i))
#define ROI_MISR_EXPECTED(i)                       (0x50 + 0x4 * (i))

/* ROI_MISR_OP_MODE register */
#define ROI_BYPASS_EN(i)                           BIT(16 + (i))
#define ROI_EN(i)                                  BIT(i)

/* ROI_MISR_CTRL register */
#define ROI_MISR_CTRL_ENABLE                       BIT(8)
#define ROI_MISR_CTRL_STATUS                       BIT(9)
#define ROI_MISR_CTRL_STATUS_CLEAR                 BIT(10)
#define ROI_MISR_CTRL_RUN_MODE                     BIT(31)

#define ROI_POSITION_VAL(x, y)                     ((x) | ((y) << 16))
#define ROI_SIZE_VAL(w, h)                         ((w) | ((h) << 16))

static void sde_hw_roi_misr_setup(struct sde_hw_roi_misr *ctx,
		struct sde_roi_misr_hw_cfg *cfg)
{
	struct sde_hw_blk_reg_map *roi_misr_c = &ctx->hw;
	struct sde_roi_misr_hw_cfg *roi_info = cfg;
	uint32_t ctrl_val = 0;
	uint32_t op_mode = 0;
	int i;

	ctrl_val = ROI_MISR_CTRL_RUN_MODE
			| ROI_MISR_CTRL_ENABLE
			| ROI_MISR_CTRL_STATUS_CLEAR;

	spin_lock(&ctx->spin_lock);

	for (i = 0; i < ROI_MISR_MAX_ROIS_PER_MISR; ++i) {
		if (i < roi_info->roi_num) {
			ctrl_val |= cfg->frame_count[i];
			SDE_REG_WRITE(roi_misr_c, ROI_MISR_POSITION(i),
				ROI_POSITION_VAL(roi_info->misr_roi_rect[i].x,
				roi_info->misr_roi_rect[i].y));

			SDE_REG_WRITE(roi_misr_c, ROI_MISR_SIZE(i),
				ROI_SIZE_VAL(roi_info->misr_roi_rect[i].w,
				roi_info->misr_roi_rect[i].h));

			SDE_REG_WRITE(roi_misr_c, ROI_MISR_EXPECTED(i),
				roi_info->golden_value[i]);

			SDE_REG_WRITE(roi_misr_c, ROI_MISR_CTRL(i), ctrl_val);

			op_mode |= ROI_EN(i);
		} else {
			SDE_REG_WRITE(roi_misr_c, ROI_MISR_POSITION(i), 0x0);
			SDE_REG_WRITE(roi_misr_c, ROI_MISR_SIZE(i), 0x0);
			SDE_REG_WRITE(roi_misr_c, ROI_MISR_EXPECTED(i), 0x0);
			SDE_REG_WRITE(roi_misr_c, ROI_MISR_CTRL(i), 0x0);
		}
	}

	SDE_REG_WRITE(roi_misr_c, ROI_MISR_OP_MODE, op_mode);

	spin_unlock(&ctx->spin_lock);
}

static void sde_hw_roi_misr_reset(struct sde_hw_roi_misr *ctx)
{
	struct sde_hw_blk_reg_map *roi_misr_c = &ctx->hw;
	int i;

	for (i = 0; i < ROI_MISR_MAX_ROIS_PER_MISR; ++i) {
		SDE_REG_WRITE(roi_misr_c, ROI_MISR_POSITION(i), 0x0);
		SDE_REG_WRITE(roi_misr_c, ROI_MISR_SIZE(i), 0x0);
		SDE_REG_WRITE(roi_misr_c, ROI_MISR_EXPECTED(i), 0x0);
		SDE_REG_WRITE(roi_misr_c, ROI_MISR_CTRL(i), 0x0);
	}

	SDE_REG_WRITE(roi_misr_c, ROI_MISR_OP_MODE, 0x0);
}

static bool sde_hw_collect_signature(struct sde_hw_roi_misr *ctx,
	int roi_idx, u32 *misr_value)
{
	struct sde_hw_blk_reg_map *roi_misr_c = &ctx->hw;
	uint32_t reg_ctrl_value = 0;
	uint32_t status = 0;

	spin_lock(&ctx->spin_lock);

	reg_ctrl_value = SDE_REG_READ(roi_misr_c,
		ROI_MISR_CTRL(roi_idx));

	status = reg_ctrl_value & ROI_MISR_CTRL_STATUS;
	if (status) {
		*misr_value = SDE_REG_READ(roi_misr_c,
			ROI_MISR_CAPTURED(roi_idx));

		SDE_REG_WRITE(roi_misr_c, ROI_MISR_CTRL(roi_idx),
			reg_ctrl_value | ROI_MISR_CTRL_STATUS_CLEAR);
	}

	spin_unlock(&ctx->spin_lock);

	return status ? true : false;
}

static struct sde_roi_misr_cfg *_roi_misr_offset(enum sde_roi_misr roi_misr,
		struct sde_mdss_cfg *m,
		void __iomem *addr,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	if (!m || !addr || !b)
		return ERR_PTR(-EINVAL);

	for (i = 0; i < m->roi_misr_count; i++) {
		if (roi_misr == m->roi_misr[i].id) {
			b->base_off = addr;
			b->blk_off = m->roi_misr[i].base;
			b->length = m->roi_misr[i].len;
			b->hwversion = m->hwversion;
			b->log_mask = SDE_DBG_MASK_ROI_MISR;
			return &m->roi_misr[i];
		}
	}

	return ERR_PTR(-EINVAL);
}

static void _setup_roi_misr_ops(struct sde_hw_roi_misr_ops *ops,
		unsigned long features)
{
	ops->setup_roi_misr = sde_hw_roi_misr_setup;
	ops->collect_roi_misr_signature = sde_hw_collect_signature;
	ops->reset_roi_misr = sde_hw_roi_misr_reset;
};

static struct sde_hw_blk_ops sde_hw_ops = {
	.start = NULL,
	.stop = NULL,
};

struct sde_hw_roi_misr *sde_hw_roi_misr_init(enum sde_roi_misr idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m)
{
	struct sde_hw_roi_misr *c;
	struct sde_roi_misr_cfg *cfg;
	int rc;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _roi_misr_offset(idx, m, addr, &c->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		kfree(c);
		return ERR_PTR(-EINVAL);
	}

	c->idx = idx;
	c->caps = cfg;
	spin_lock_init(&c->spin_lock);
	_setup_roi_misr_ops(&c->ops, c->caps->features);

	rc = sde_hw_blk_init(&c->base, SDE_HW_BLK_ROI_MISR, idx, &sde_hw_ops);
	if (rc) {
		SDE_ERROR("failed to init hw blk %d\n", rc);
		goto blk_init_error;
	}

	sde_dbg_reg_register_dump_range(SDE_DBG_NAME, cfg->name, c->hw.blk_off,
		c->hw.blk_off + c->hw.length, c->hw.xin_id);

	return c;

blk_init_error:
	kzfree(c);

	return ERR_PTR(rc);
}

void sde_hw_roi_misr_destroy(struct sde_hw_roi_misr *roi_misr)
{
	if (roi_misr)
		sde_hw_blk_destroy(&roi_misr->base);
	kfree(roi_misr);
}

