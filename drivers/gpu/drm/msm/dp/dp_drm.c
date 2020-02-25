/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"[drm-dp]: %s: " fmt, __func__

#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_crtc.h>

#include "msm_drv.h"
#include "msm_kms.h"
#include "sde_connector.h"
#include "dp_drm.h"
#include "dp_debug.h"

#define DP_MST_DEBUG(fmt, ...) pr_debug(fmt, ##__VA_ARGS__)

struct dp_bond_bridge {
	struct drm_bridge base;
	struct drm_encoder *encoder;
	struct dp_display *display;
	struct dp_bridge *bridges[MAX_DP_BOND_NUM];
	u32 bridge_num;
	enum dp_bond_type type;
	u32 bond_mask;
};

struct dp_bond_mgr {
	struct drm_private_obj obj;
	struct dp_bond_bridge bond_bridge[DP_BOND_MAX];
};

struct dp_bond_mgr_state {
	struct drm_private_state base;
	struct drm_connector *connector[DP_BOND_MAX];
	u32 bond_mask[DP_BOND_MAX];
	u32 connector_mask;
};

struct dp_bond_info {
	struct dp_bond_mgr *bond_mgr;
	struct dp_bond_bridge *bond_bridge[DP_BOND_MAX];
	u32 bond_idx;
};

#define to_dp_bridge(x)     container_of((x), struct dp_bridge, base)

#define to_dp_bond_bridge(x)     container_of((x), struct dp_bond_bridge, base)

#define to_dp_bond_mgr(x)     container_of((x), struct dp_bond_mgr, base)

#define to_dp_bond_mgr_state(x) \
		container_of((x), struct dp_bond_mgr_state, base)

static struct drm_private_state *dp_bond_duplicate_mgr_state(
		struct drm_private_obj *obj)
{
	struct dp_bond_mgr_state *state;

	state = kmemdup(obj->state, sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	__drm_atomic_helper_private_obj_duplicate_state(obj, &state->base);

	return &state->base;
}

static void dp_bond_destroy_mgr_state(struct drm_private_obj *obj,
		struct drm_private_state *state)
{
	struct dp_bond_mgr_state *bond_state =
			to_dp_bond_mgr_state(state);

	kfree(bond_state);
}

static const struct drm_private_state_funcs dp_bond_mgr_state_funcs = {
	.atomic_duplicate_state = dp_bond_duplicate_mgr_state,
	.atomic_destroy_state = dp_bond_destroy_mgr_state,
};

static struct dp_bond_mgr_state *dp_bond_get_mgr_atomic_state(
		struct drm_atomic_state *state, struct dp_bond_mgr *mgr)
{
	return to_dp_bond_mgr_state(
		drm_atomic_get_private_obj_state(state, &mgr->obj));
}

static inline bool dp_bond_is_tile_mode(const struct drm_display_mode *mode)
{
	return !!(mode->flags & DRM_MODE_FLAG_CLKDIV2);
}

static inline void
dp_bond_split_tile_timing(struct drm_display_mode *mode, int num_h_tile)
{
	mode->hdisplay /= num_h_tile;
	mode->hsync_start /= num_h_tile;
	mode->hsync_end /= num_h_tile;
	mode->htotal /= num_h_tile;
	mode->hskew /= num_h_tile;
	mode->clock /= num_h_tile;
	mode->flags &= ~DRM_MODE_FLAG_CLKDIV2;
}

static inline void
dp_bond_merge_tile_timing(struct drm_display_mode *mode, int num_h_tile)
{
	mode->hdisplay *= num_h_tile;
	mode->hsync_start *= num_h_tile;
	mode->hsync_end *= num_h_tile;
	mode->htotal *= num_h_tile;
	mode->hskew *= num_h_tile;
	mode->clock *= num_h_tile;
	mode->flags |= DRM_MODE_FLAG_CLKDIV2;
}

void convert_to_drm_mode(const struct dp_display_mode *dp_mode,
				struct drm_display_mode *drm_mode)
{
	u32 flags = 0;

	memset(drm_mode, 0, sizeof(*drm_mode));

	drm_mode->hdisplay = dp_mode->timing.h_active;
	drm_mode->hsync_start = drm_mode->hdisplay +
				dp_mode->timing.h_front_porch;
	drm_mode->hsync_end = drm_mode->hsync_start +
			      dp_mode->timing.h_sync_width;
	drm_mode->htotal = drm_mode->hsync_end + dp_mode->timing.h_back_porch;
	drm_mode->hskew = dp_mode->timing.h_skew;

	drm_mode->vdisplay = dp_mode->timing.v_active;
	drm_mode->vsync_start = drm_mode->vdisplay +
				dp_mode->timing.v_front_porch;
	drm_mode->vsync_end = drm_mode->vsync_start +
			      dp_mode->timing.v_sync_width;
	drm_mode->vtotal = drm_mode->vsync_end + dp_mode->timing.v_back_porch;

	drm_mode->vrefresh = dp_mode->timing.refresh_rate;
	drm_mode->clock = dp_mode->timing.pixel_clk_khz;

	if (dp_mode->timing.h_active_low)
		flags |= DRM_MODE_FLAG_NHSYNC;
	else
		flags |= DRM_MODE_FLAG_PHSYNC;

	if (dp_mode->timing.v_active_low)
		flags |= DRM_MODE_FLAG_NVSYNC;
	else
		flags |= DRM_MODE_FLAG_PVSYNC;

	drm_mode->flags = flags;

	drm_mode->type = 0x48;
	drm_mode_set_name(drm_mode);
}

static int dp_bridge_attach(struct drm_bridge *dp_bridge)
{
	struct dp_bridge *bridge = to_dp_bridge(dp_bridge);

	if (!dp_bridge) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	pr_debug("[%d] attached\n", bridge->id);

	return 0;
}

static void dp_bridge_pre_enable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_bridge *bridge;
	struct dp_display *dp;

	if (!drm_bridge) {
		pr_err("Invalid params\n");
		return;
	}

	bridge = to_dp_bridge(drm_bridge);
	dp = bridge->display;

	if (!bridge->connector) {
		pr_err("Invalid connector\n");
		return;
	}

	if (!bridge->dp_panel) {
		pr_err("Invalid dp_panel\n");
		return;
	}

	/*
	 * Non-bond mode, associated with the CRTC,
	 * set non-bond mode to the display
	 */
	if (bridge->base.encoder->crtc != NULL)
		dp->set_phy_bond_mode(dp, DP_PHY_BOND_MODE_NONE);

	/* By this point mode should have been validated through mode_fixup */
	rc = dp->set_mode(dp, bridge->dp_panel, &bridge->dp_mode);
	if (rc) {
		pr_err("[%d] failed to perform a mode set, rc=%d\n",
		       bridge->id, rc);
		return;
	}

	rc = dp->prepare(dp, bridge->dp_panel);
	if (rc) {
		pr_err("[%d] DP display prepare failed, rc=%d\n",
		       bridge->id, rc);
		return;
	}

	/* for SST force stream id, start slot and total slots to 0 */
	dp->set_stream_info(dp, bridge->dp_panel, 0, 0, 0, 0, 0);

	rc = dp->enable(dp, bridge->dp_panel);
	if (rc) {
		pr_err("[%d] DP display enable failed, rc=%d\n",
		       bridge->id, rc);
		dp->unprepare(dp, bridge->dp_panel);
	}
}

static void dp_bridge_enable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_bridge *bridge;
	struct dp_display *dp;

	if (!drm_bridge) {
		pr_err("Invalid params\n");
		return;
	}

	bridge = to_dp_bridge(drm_bridge);
	if (!bridge->connector) {
		pr_err("Invalid connector\n");
		return;
	}

	if (!bridge->dp_panel) {
		pr_err("Invalid dp_panel\n");
		return;
	}

	dp = bridge->display;

	rc = dp->post_enable(dp, bridge->dp_panel);
	if (rc)
		pr_err("[%d] DP display post enable failed, rc=%d\n",
		       bridge->id, rc);
}

static void dp_bridge_disable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_bridge *bridge;
	struct dp_display *dp;

	if (!drm_bridge) {
		pr_err("Invalid params\n");
		return;
	}

	bridge = to_dp_bridge(drm_bridge);
	if (!bridge->connector) {
		pr_err("Invalid connector\n");
		return;
	}

	if (!bridge->dp_panel) {
		pr_err("Invalid dp_panel\n");
		return;
	}

	dp = bridge->display;

	if (!dp) {
		pr_err("dp is null\n");
		return;
	}

	if (dp)
		sde_connector_helper_bridge_disable(bridge->connector);

	rc = dp->pre_disable(dp, bridge->dp_panel);
	if (rc) {
		pr_err("[%d] DP display pre disable failed, rc=%d\n",
		       bridge->id, rc);
	}
}

static void dp_bridge_post_disable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_bridge *bridge;
	struct dp_display *dp;

	if (!drm_bridge) {
		pr_err("Invalid params\n");
		return;
	}

	bridge = to_dp_bridge(drm_bridge);
	if (!bridge->connector) {
		pr_err("Invalid connector\n");
		return;
	}

	if (!bridge->dp_panel) {
		pr_err("Invalid dp_panel\n");
		return;
	}

	dp = bridge->display;

	rc = dp->disable(dp, bridge->dp_panel);
	if (rc) {
		pr_err("[%d] DP display disable failed, rc=%d\n",
		       bridge->id, rc);
		return;
	}

	rc = dp->unprepare(dp, bridge->dp_panel);
	if (rc) {
		pr_err("[%d] DP display unprepare failed, rc=%d\n",
		       bridge->id, rc);
		return;
	}
}

static void dp_bridge_mode_set(struct drm_bridge *drm_bridge,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct dp_bridge *bridge;
	struct dp_display *dp;

	if (!drm_bridge || !mode || !adjusted_mode) {
		pr_err("Invalid params\n");
		return;
	}

	bridge = to_dp_bridge(drm_bridge);
	if (!bridge->connector) {
		pr_err("Invalid connector\n");
		return;
	}

	if (!bridge->dp_panel) {
		pr_err("Invalid dp_panel\n");
		return;
	}

	dp = bridge->display;

	dp->convert_to_dp_mode(dp, bridge->dp_panel, adjusted_mode,
			&bridge->dp_mode);
}

static bool dp_bridge_mode_fixup(struct drm_bridge *drm_bridge,
				  const struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	bool ret = true;
	struct dp_display_mode dp_mode;
	struct dp_bridge *bridge;
	struct dp_display *dp;

	if (!drm_bridge || !mode || !adjusted_mode) {
		pr_err("Invalid params\n");
		ret = false;
		goto end;
	}

	bridge = to_dp_bridge(drm_bridge);
	if (!bridge->connector) {
		pr_err("Invalid connector\n");
		ret = false;
		goto end;
	}

	if (!bridge->dp_panel) {
		pr_err("Invalid dp_panel\n");
		ret = false;
		goto end;
	}

	dp = bridge->display;

	if (dp_bond_is_tile_mode(mode)) {
		struct drm_display_mode tmp;

		tmp = *mode;
		dp_bond_split_tile_timing(&tmp, dp->base_connector->num_h_tile);
		dp->convert_to_dp_mode(dp, bridge->dp_panel, &tmp, &dp_mode);
		convert_to_drm_mode(&dp_mode, adjusted_mode);
		dp_bond_merge_tile_timing(adjusted_mode,
				dp->base_connector->num_h_tile);
		goto end;
	}

	dp->convert_to_dp_mode(dp, bridge->dp_panel, mode, &dp_mode);
	convert_to_drm_mode(&dp_mode, adjusted_mode);
end:
	return ret;
}

static const struct drm_bridge_funcs dp_bridge_ops = {
	.attach       = dp_bridge_attach,
	.mode_fixup   = dp_bridge_mode_fixup,
	.pre_enable   = dp_bridge_pre_enable,
	.enable       = dp_bridge_enable,
	.disable      = dp_bridge_disable,
	.post_disable = dp_bridge_post_disable,
	.mode_set     = dp_bridge_mode_set,
};

static bool dp_bond_bridge_mode_fixup(struct drm_bridge *drm_bridge,
				  const struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	struct dp_bond_bridge *bridge;

	if (!drm_bridge) {
		pr_err("Invalid params\n");
		return false;
	}

	bridge = to_dp_bond_bridge(drm_bridge);

	return drm_bridge_mode_fixup(&bridge->bridges[0]->base,
			mode, adjusted_mode);
}

static void dp_bond_bridge_pre_enable(struct drm_bridge *drm_bridge)
{
	struct dp_bond_bridge *bridge;
	int i;

	if (!drm_bridge) {
		pr_err("Invalid params\n");
		return;
	}

	bridge = to_dp_bond_bridge(drm_bridge);

	/* Set the corresponding bond mode to bonded displays */
	for (i = 0; i < bridge->bridge_num; i++) {
		enum dp_phy_bond_mode mode;

		if (i == 0) {
			if (bridge->bridge_num == 2)
				mode = DP_PHY_BOND_MODE_PLL_MASTER;
			else
				mode = DP_PHY_BOND_MODE_PCLK_MASTER;
		} else {
			if (bridge->bridge_num == 2)
				mode = DP_PHY_BOND_MODE_PLL_SLAVE;
			else
				mode = DP_PHY_BOND_MODE_PCLK_SLAVE;
		}
		if (bridge->bridges[i]->display)
			bridge->bridges[i]->display->set_phy_bond_mode(
					bridge->bridges[i]->display, mode);
	}

	/* In the order of from master PHY to slave PHY */
	for (i = 0; i < bridge->bridge_num; i++)
		drm_bridge_pre_enable(&bridge->bridges[i]->base);
}

static void dp_bond_bridge_enable(struct drm_bridge *drm_bridge)
{
	struct dp_bond_bridge *bridge;
	int i;

	if (!drm_bridge) {
		pr_err("Invalid params\n");
		return;
	}

	bridge = to_dp_bond_bridge(drm_bridge);

	/* In the order of from master PHY to slave PHY */
	for (i = 0; i < bridge->bridge_num; i++)
		drm_bridge_enable(&bridge->bridges[i]->base);
}

static void dp_bond_bridge_disable(struct drm_bridge *drm_bridge)
{
	struct dp_bond_bridge *bridge;
	int i;

	if (!drm_bridge) {
		pr_err("Invalid params\n");
		return;
	}

	bridge = to_dp_bond_bridge(drm_bridge);

	/* In the order of from slave PHY to master PHY */
	for (i = bridge->bridge_num - 1; i >= 0; i--)
		drm_bridge_disable(&bridge->bridges[i]->base);
}

static void dp_bond_bridge_post_disable(struct drm_bridge *drm_bridge)
{
	struct dp_bond_bridge *bridge;
	int i;

	if (!drm_bridge) {
		pr_err("Invalid params\n");
		return;
	}

	bridge = to_dp_bond_bridge(drm_bridge);

	/* In the order of from slave PHY to master PHY */
	for (i = bridge->bridge_num - 1; i >= 0; i--)
		drm_bridge_post_disable(&bridge->bridges[i]->base);
}

static void dp_bond_bridge_mode_set(struct drm_bridge *drm_bridge,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct dp_bond_bridge *bridge;
	struct drm_display_mode tmp;
	int i;

	if (!drm_bridge) {
		pr_err("Invalid params\n");
		return;
	}

	bridge = to_dp_bond_bridge(drm_bridge);

	tmp = *adjusted_mode;
	dp_bond_split_tile_timing(&tmp, bridge->bridge_num);

	for (i = 0; i < bridge->bridge_num; i++)
		drm_bridge_mode_set(&bridge->bridges[i]->base, &tmp, &tmp);
}

static const struct drm_bridge_funcs dp_bond_bridge_ops = {
	.mode_fixup   = dp_bond_bridge_mode_fixup,
	.pre_enable   = dp_bond_bridge_pre_enable,
	.enable       = dp_bond_bridge_enable,
	.disable      = dp_bond_bridge_disable,
	.post_disable = dp_bond_bridge_post_disable,
	.mode_set     = dp_bond_bridge_mode_set,
};

static inline
enum dp_bond_type dp_bond_get_bond_type(struct drm_connector *connector)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct dp_display *dp_display = c_conn->display;
	struct dp_bond_info *bond_info = dp_display->dp_bond_prv_info;
	enum dp_bond_type type;

	if (!dp_display->dp_bond_prv_info || !connector->has_tile)
		return DP_BOND_MAX;

	type = connector->num_h_tile - 2;
	if (type < 0 || type >= DP_BOND_MAX ||
			!bond_info->bond_bridge[type])
		return DP_BOND_MAX;

	return type;
}

static inline bool dp_bond_is_primary(struct dp_display *dp_display,
		enum dp_bond_type type)
{
	struct dp_bond_info *bond_info = dp_display->dp_bond_prv_info;
	struct dp_bond_bridge *bond_bridge;

	if (!bond_info)
		return false;

	bond_bridge = bond_info->bond_bridge[type];
	if (!bond_bridge)
		return false;

	return bond_bridge->display == dp_display;
}

static void dp_bond_fixup_tile_mode(struct drm_connector *connector)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct dp_display *dp_display = c_conn->display;
	struct drm_display_mode *mode, *newmode;
	struct list_head tile_modes;
	enum dp_bond_type type;
	struct dp_bond_info *bond_info;
	struct dp_bond_bridge *bond_bridge;
	int i;

	/* checks supported tiling mode */
	type = dp_bond_get_bond_type(connector);
	if (type == DP_BOND_MAX)
		return;

	INIT_LIST_HEAD(&tile_modes);

	list_for_each_entry(mode, &connector->probed_modes, head) {
		if (!dp_display->force_bond_mode &&
			(mode->hdisplay != connector->tile_h_size ||
			mode->vdisplay != connector->tile_v_size))
			continue;

		newmode = drm_mode_duplicate(connector->dev, mode);
		if (!newmode)
			break;

		dp_bond_merge_tile_timing(newmode, connector->num_h_tile);
		newmode->type |= DRM_MODE_TYPE_PREFERRED;
		drm_mode_set_name(newmode);

		list_add_tail(&newmode->head, &tile_modes);
	}

	list_for_each_entry_safe(mode, newmode, &tile_modes, head) {
		list_del(&mode->head);
		list_add_tail(&mode->head, &connector->probed_modes);
	}

	/* update display info for sibling connectors */
	bond_info = dp_display->dp_bond_prv_info;
	bond_bridge = bond_info->bond_bridge[type];
	for (i = 0; i < bond_bridge->bridge_num; i++) {
		if (bond_bridge->bridges[i]->connector == connector)
			continue;
		bond_bridge->bridges[i]->connector->display_info =
				connector->display_info;
	}
}

static bool dp_bond_check_connector(struct drm_connector *connector,
		enum dp_bond_type type)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct dp_display *dp_display = c_conn->display, *p;
	struct dp_bond_info *bond_info = dp_display->dp_bond_prv_info;
	struct dp_bond_bridge *bond_bridge;
	struct drm_connector *p_conn;
	int i;

	bond_bridge = bond_info->bond_bridge[type];
	if (!bond_bridge)
		return false;

	for (i = 0; i < bond_bridge->bridge_num; i++) {
		if (bond_bridge->bridges[i]->connector == connector)
			continue;

		p = bond_bridge->bridges[i]->display;
		if (!p->is_sst_connected)
			return false;

		if (dp_display->force_bond_mode) {
			if (p->force_bond_mode)
				continue;
			else
				return false;
		}

		p_conn = p->base_connector;
		if (!p_conn->has_tile || !p_conn->tile_group ||
			p_conn->tile_group->id != connector->tile_group->id)
			return false;
	}

	return true;
}

static void dp_bond_check_force_mode(struct drm_connector *connector)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct dp_display *dp_display = c_conn->display;
	enum dp_bond_type type, preferred_type = DP_BOND_MAX;

	if (!dp_display->dp_bond_prv_info || !dp_display->force_bond_mode)
		return;

	if (connector->has_tile && connector->tile_group)
		return;

	connector->has_tile = false;

	for (type = DP_BOND_DUAL; type < DP_BOND_MAX; type++) {
		if (!dp_bond_check_connector(connector, type))
			continue;

		preferred_type = type;
	}

	if (preferred_type == DP_BOND_MAX)
		return;

	connector->has_tile = true;
	connector->num_h_tile = preferred_type + 2;
	connector->num_v_tile = 1;
}

int dp_connector_config_hdr(struct drm_connector *connector, void *display,
	struct sde_connector_state *c_state)
{
	struct dp_display *dp = display;
	struct sde_connector *sde_conn;

	if (!display || !c_state || !connector) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	sde_conn = to_sde_connector(connector);
	if (!sde_conn->drv_panel) {
		pr_err("invalid dp panel\n");
		return -EINVAL;
	}

	return dp->config_hdr(dp, sde_conn->drv_panel, &c_state->hdr_meta);
}

int dp_connector_post_init(struct drm_connector *connector, void *display)
{
	int rc;
	struct dp_display *dp_display = display;
	struct sde_connector *sde_conn;

	if (!dp_display || !connector)
		return -EINVAL;

	dp_display->base_connector = connector;
	dp_display->bridge->connector = connector;

	if (dp_display->post_init) {
		rc = dp_display->post_init(dp_display);
		if (rc)
			goto end;
	}

	sde_conn = to_sde_connector(connector);
	dp_display->bridge->dp_panel = sde_conn->drv_panel;

	rc = dp_mst_init(dp_display);
end:
	return rc;
}

int dp_connector_get_mode_info(struct drm_connector *connector,
		const struct drm_display_mode *drm_mode,
		struct msm_mode_info *mode_info,
		u32 max_mixer_width, void *display)
{
	const u32 single_intf = 1;
	const u32 no_enc = 0;
	struct msm_display_topology *topology;
	struct sde_connector *sde_conn;
	struct dp_panel *dp_panel;
	struct dp_display_mode dp_mode;
	struct dp_display *dp_disp = display;
	struct msm_drm_private *priv;
	int rc = 0;

	if (!drm_mode || !mode_info || !max_mixer_width || !connector ||
			!display) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	if (dp_bond_is_tile_mode(drm_mode)) {
		struct drm_display_mode tmp;

		tmp = *drm_mode;
		dp_bond_split_tile_timing(&tmp, connector->num_h_tile);

		/* Get single tile mode info */
		rc = dp_connector_get_mode_info(connector, &tmp, mode_info,
				max_mixer_width, display);
		if (rc)
			return rc;

		mode_info->topology.num_intf *= connector->num_h_tile;
		mode_info->topology.num_lm *= connector->num_h_tile;
		mode_info->topology.num_enc *= connector->num_h_tile;
		return 0;
	}

	memset(mode_info, 0, sizeof(*mode_info));

	sde_conn = to_sde_connector(connector);
	dp_panel = sde_conn->drv_panel;
	priv = connector->dev->dev_private;

	topology = &mode_info->topology;

	rc = msm_get_mixer_count(priv, drm_mode, max_mixer_width,
			&topology->num_lm);
	if (rc) {
		pr_err("error getting mixer count, rc:%d\n", rc);
		return rc;
	}

	topology->num_enc = no_enc;
	topology->num_intf = single_intf;

	mode_info->frame_rate = drm_mode->vrefresh;
	mode_info->vtotal = drm_mode->vtotal;

	mode_info->wide_bus_en = dp_panel->widebus_en;

	dp_disp->convert_to_dp_mode(dp_disp, dp_panel, drm_mode, &dp_mode);

	if (dp_mode.timing.comp_info.comp_ratio) {
		memcpy(&mode_info->comp_info,
			&dp_mode.timing.comp_info,
			sizeof(mode_info->comp_info));

		topology->num_enc = topology->num_lm;
	}

	return 0;
}

int dp_connector_get_info(struct drm_connector *connector,
		struct msm_display_info *info, void *data)
{
	struct dp_display *display = data;

	if (!info || !display || !display->drm_dev) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	info->intf_type = DRM_MODE_CONNECTOR_DisplayPort;

	if (!display->bridge) {
		struct dp_display_info dp_info = {0};
		int rc, i;

		rc = dp_display_get_info(display, &dp_info);
		if (rc) {
			pr_err("failed to get info\n");
			return rc;
		}

		info->num_of_h_tiles = 1;
		for (i = 0; i < DP_STREAM_MAX; i++)
			info->h_tile_instance[i] = dp_info.intf_idx[i];
	}

	info->is_connected = display->is_sst_connected;
	info->capabilities = MSM_DISPLAY_CAP_VID_MODE | MSM_DISPLAY_CAP_EDID |
		MSM_DISPLAY_CAP_HOT_PLUG;

	return 0;
}

enum drm_connector_status dp_connector_detect(struct drm_connector *conn,
		bool force,
		void *display)
{
	enum drm_connector_status status = connector_status_unknown;
	struct msm_display_info info;
	struct dp_display *dp_display;
	int rc;

	if (!conn || !display)
		return status;

	/* get display dp_info */
	memset(&info, 0x0, sizeof(info));
	rc = dp_connector_get_info(conn, &info, display);
	if (rc) {
		pr_err("failed to get display info, rc=%d\n", rc);
		return connector_status_disconnected;
	}

	if (info.capabilities & MSM_DISPLAY_CAP_HOT_PLUG)
		status = (info.is_connected ? connector_status_connected :
					      connector_status_disconnected);
	else
		status = connector_status_connected;

	conn->display_info.width_mm = info.width_mm;
	conn->display_info.height_mm = info.height_mm;

	/*
	 * hide tiled connectors so only primary connector
	 * is reported to user
	 */
	dp_display = display;
	if (dp_display->dp_bond_prv_info &&
			status == connector_status_connected) {
		enum dp_bond_type type;

		dp_bond_check_force_mode(conn);

		type = dp_bond_get_bond_type(conn);
		if (type == DP_BOND_MAX)
			return status;

		if (!dp_bond_is_primary(dp_display, type)) {
			if (dp_bond_check_connector(conn, type))
				status = connector_status_disconnected;
		}
	}

	return status;
}

void dp_connector_post_open(struct drm_connector *connector, void *display)
{
	struct dp_display *dp;

	if (!display) {
		pr_err("invalid input\n");
		return;
	}

	dp = display;

	if (dp->post_open)
		dp->post_open(dp);
}

int dp_connector_get_modes(struct drm_connector *connector,
		void *display)
{
	int rc = 0;
	struct dp_display *dp;
	struct dp_display_mode *dp_mode = NULL;
	struct drm_display_mode *m, drm_mode;
	struct sde_connector *sde_conn;

	if (!connector || !display)
		return 0;

	sde_conn = to_sde_connector(connector);
	if (!sde_conn->drv_panel) {
		pr_err("invalid dp panel\n");
		return 0;
	}

	dp = display;

	dp_mode = kzalloc(sizeof(*dp_mode),  GFP_KERNEL);
	if (!dp_mode)
		return 0;

	/* pluggable case assumes EDID is read when HPD */
	if (dp->is_sst_connected) {
		rc = dp->get_modes(dp, sde_conn->drv_panel, dp_mode);
		if (!rc)
			pr_err("failed to get DP sink modes, rc=%d\n", rc);

		if (dp_mode->timing.pixel_clk_khz) { /* valid DP mode */
			memset(&drm_mode, 0x0, sizeof(drm_mode));
			convert_to_drm_mode(dp_mode, &drm_mode);
			m = drm_mode_duplicate(connector->dev, &drm_mode);
			if (!m) {
				pr_err("failed to add mode %ux%u\n",
				       drm_mode.hdisplay,
				       drm_mode.vdisplay);
				kfree(dp_mode);
				return 0;
			}
			m->width_mm = connector->display_info.width_mm;
			m->height_mm = connector->display_info.height_mm;
			drm_mode_probed_add(connector, m);
		}

		if (dp->dp_bond_prv_info)
			dp_bond_fixup_tile_mode(connector);
	} else {
		pr_err("No sink connected\n");
	}
	kfree(dp_mode);

	return rc;
}

int dp_connnector_set_info_blob(struct drm_connector *connector,
		void *info, void *display, struct msm_mode_info *mode_info)
{
	struct dp_display *dp_display = display;
	const char *display_type = NULL;

	dp_display->get_display_type(dp_display, &display_type);
	sde_kms_info_add_keystr(info,
		"display type", display_type);

	return 0;
}

int dp_drm_bridge_init(void *data, struct drm_encoder *encoder)
{
	int rc = 0;
	struct dp_bridge *bridge;
	struct drm_device *dev;
	struct dp_display *display = data;
	struct msm_drm_private *priv = NULL;

	bridge = kzalloc(sizeof(*bridge), GFP_KERNEL);
	if (!bridge) {
		rc = -ENOMEM;
		goto error;
	}

	dev = display->drm_dev;
	bridge->display = display;
	bridge->base.funcs = &dp_bridge_ops;
	bridge->base.encoder = encoder;

	priv = dev->dev_private;

	rc = drm_bridge_attach(encoder, &bridge->base, NULL);
	if (rc) {
		pr_err("failed to attach bridge, rc=%d\n", rc);
		goto error_free_bridge;
	}

	rc = display->request_irq(display);
	if (rc) {
		pr_err("request_irq failed, rc=%d\n", rc);
		goto error_free_bridge;
	}

	encoder->bridge = &bridge->base;
	priv->bridges[priv->num_bridges++] = &bridge->base;
	display->bridge = bridge;

	return 0;
error_free_bridge:
	kfree(bridge);
error:
	return rc;
}

void dp_drm_bridge_deinit(void *data)
{
	struct dp_display *display = data;
	struct dp_bridge *bridge = display->bridge;

	if (bridge && bridge->base.encoder)
		bridge->base.encoder->bridge = NULL;

	kfree(bridge);
}

enum drm_mode_status dp_connector_mode_valid(struct drm_connector *connector,
		struct drm_display_mode *mode, void *display)
{
	struct dp_display *dp_disp;
	struct sde_connector *sde_conn;

	if (!mode || !display || !connector) {
		pr_err("invalid params\n");
		return MODE_ERROR;
	}

	sde_conn = to_sde_connector(connector);
	if (!sde_conn->drv_panel) {
		pr_err("invalid dp panel\n");
		return MODE_ERROR;
	}

	dp_disp = display;
	mode->vrefresh = drm_mode_vrefresh(mode);

	if (dp_disp->dp_bond_prv_info && dp_bond_is_tile_mode(mode)) {
		struct drm_display_mode tmp;
		enum dp_bond_type type;

		type = dp_bond_get_bond_type(connector);
		if (type == DP_BOND_MAX)
			return MODE_BAD;

		if (!dp_bond_check_connector(connector, type)) {
			pr_debug("mode:%s requires multi ports\n", mode->name);
			return MODE_BAD;
		}

		tmp = *mode;
		dp_bond_split_tile_timing(&tmp, connector->num_h_tile);

		return dp_disp->validate_mode(dp_disp,
				sde_conn->drv_panel, &tmp);
	}

	return dp_disp->validate_mode(dp_disp, sde_conn->drv_panel, mode);
}

int dp_connector_update_pps(struct drm_connector *connector,
		char *pps_cmd, void *display)
{
	struct dp_display *dp_disp;
	struct sde_connector *sde_conn;

	if (!display || !connector) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	sde_conn = to_sde_connector(connector);
	if (!sde_conn->drv_panel) {
		pr_err("invalid dp panel\n");
		return MODE_ERROR;
	}

	dp_disp = display;
	return dp_disp->update_pps(dp_disp, connector, pps_cmd);
}

int dp_drm_bond_bridge_init(void *display,
	struct drm_encoder *encoder,
	enum dp_bond_type type,
	struct dp_display_bond_displays *bond_displays)
{
	struct dp_display *dp_display = display, *bond_display;
	struct dp_bond_info *bond_info;
	struct dp_bond_bridge *bridge;
	struct dp_bond_mgr *mgr;
	struct dp_bond_mgr_state *state;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	int i, rc;

	if (type < 0 || type >= DP_BOND_MAX || !bond_displays ||
		bond_displays->dp_display_num >= MAX_DP_BOND_NUM)
		return -EINVAL;

	priv = dp_display->drm_dev->dev_private;
	sde_kms = to_sde_kms(priv->kms);

	mgr = sde_kms->dp_bond_mgr;
	if (!mgr) {
		mgr = devm_kzalloc(dp_display->drm_dev->dev,
				sizeof(*mgr), GFP_KERNEL);
		if (!mgr)
			return -ENOMEM;

		state = kzalloc(sizeof(*state), GFP_KERNEL);
		if (state == NULL)
			return -ENOMEM;

		drm_atomic_private_obj_init(&mgr->obj,
					    &state->base,
					    &dp_bond_mgr_state_funcs);
		sde_kms->dp_bond_mgr = mgr;
	}

	for (i = 0; i < bond_displays->dp_display_num; i++) {
		bond_display = bond_displays->dp_display[i];
		if (!bond_display->dp_bond_prv_info) {
			bond_info = devm_kzalloc(
				dp_display->drm_dev->dev,
				sizeof(*bond_info), GFP_KERNEL);
			if (!bond_info)
				return -ENOMEM;
			bond_info->bond_mgr = mgr;
			bond_info->bond_idx = drm_connector_index(
					bond_display->base_connector);
			bond_display->dp_bond_prv_info = bond_info;
		}
	}

	bond_info = dp_display->dp_bond_prv_info;
	if (!bond_info)
		return -EINVAL;

	bridge = &mgr->bond_bridge[type];
	if (bridge->display) {
		pr_err("bond bridge already inited\n");
		return -EINVAL;
	}

	bridge->encoder = encoder;
	bridge->base.funcs = &dp_bond_bridge_ops;
	bridge->base.encoder = encoder;
	bridge->display = display;
	bridge->type = type;
	bridge->bridge_num = bond_displays->dp_display_num;

	for (i = 0; i < bridge->bridge_num; i++) {
		bond_display = bond_displays->dp_display[i];
		bond_info = bond_display->dp_bond_prv_info;
		bond_info->bond_bridge[type] = bridge;
		bridge->bond_mask |= (1 << bond_info->bond_idx);
		bridge->bridges[i] = bond_display->bridge;
	}

	rc = drm_bridge_attach(encoder, &bridge->base, NULL);
	if (rc) {
		pr_err("failed to attach bridge, rc=%d\n", rc);
		return rc;
	}

	encoder->bridge = &bridge->base;
	priv->bridges[priv->num_bridges++] = &bridge->base;

	return 0;
}

struct drm_encoder *dp_connector_atomic_best_encoder(
		struct drm_connector *connector, void *display,
		struct drm_connector_state *state)
{
	struct dp_display *dp_display = display;
	struct drm_crtc_state *crtc_state;
	struct sde_connector *sde_conn = to_sde_connector(connector);
	struct dp_bond_info *bond_info;
	struct dp_bond_bridge *bond_bridge;
	struct dp_bond_mgr *bond_mgr;
	struct dp_bond_mgr_state *bond_state;
	enum dp_bond_type type;

	/* return if bond mode is not supported */
	if (!dp_display->dp_bond_prv_info)
		return sde_conn->encoder;

	/* get current mode */
	crtc_state = drm_atomic_get_new_crtc_state(state->state, state->crtc);

	/* return encoder in state if there is no switch needed */
	if (state->best_encoder) {
		if (dp_bond_is_tile_mode(&crtc_state->mode)) {
			if (state->best_encoder != sde_conn->encoder)
				return state->best_encoder;
		} else {
			if (state->best_encoder == sde_conn->encoder)
				return state->best_encoder;
		}
	}

	bond_info = dp_display->dp_bond_prv_info;
	bond_mgr = bond_info->bond_mgr;
	bond_state = dp_bond_get_mgr_atomic_state(state->state, bond_mgr);
	if (IS_ERR(bond_state))
		return NULL;

	/* clear bond connector */
	for (type = 0; type < DP_BOND_MAX; type++) {
		if (bond_state->connector[type] != connector) {
			if (bond_state->bond_mask[type] &
					(1 << bond_info->bond_idx)) {
				pr_debug("single encoder is in use\n");
				return NULL;
			}
			continue;
		}

		bond_bridge = bond_info->bond_bridge[type];
		bond_state->connector_mask &= ~bond_bridge->bond_mask;
		bond_state->bond_mask[type] = 0;
		bond_state->connector[type] = NULL;
		break;
	}

	/* clear single connector */
	bond_state->connector_mask &= ~(1 << bond_info->bond_idx);

	if (dp_bond_is_tile_mode(&crtc_state->mode)) {
		type = dp_bond_get_bond_type(connector);
		if (type == DP_BOND_MAX)
			return NULL;

		if (!dp_bond_check_connector(connector, type))
			return NULL;

		bond_bridge = bond_info->bond_bridge[type];
		if (bond_state->connector_mask & bond_bridge->bond_mask) {
			pr_debug("bond encoder is in use\n");
			return NULL;
		}

		bond_state->connector_mask |= bond_bridge->bond_mask;
		bond_state->bond_mask[type] = bond_bridge->bond_mask;
		bond_state->connector[type] = connector;
		return bond_bridge->encoder;
	}

	bond_state->connector_mask |= (1 << bond_info->bond_idx);
	return sde_conn->encoder;
}

int dp_connector_atomic_check(struct drm_connector *connector,
		void *display, struct drm_connector_state *new_conn_state)
{
	struct drm_atomic_state *state;
	struct drm_connector_state *old_conn_state;
	struct drm_crtc *old_crtc;
	struct drm_crtc_state *crtc_state;
	struct dp_display *dp_display = display;
	struct dp_bond_info *bond_info;
	struct dp_bond_bridge *bond_bridge;
	struct dp_bond_mgr_state *bond_state;

	/* return if bond mode is not supported */
	if (!dp_display->dp_bond_prv_info)
		return 0;

	if (!new_conn_state)
		return 0;

	state = new_conn_state->state;

	old_conn_state = drm_atomic_get_old_connector_state(state, connector);
	if (!old_conn_state)
		return 0;

	old_crtc = old_conn_state->crtc;
	if (!old_crtc)
		return 0;

	crtc_state = drm_atomic_get_new_crtc_state(state, old_crtc);

	if (drm_atomic_crtc_needs_modeset(crtc_state) &&
			!new_conn_state->crtc) {
		bond_info = dp_display->dp_bond_prv_info;
		bond_state = dp_bond_get_mgr_atomic_state(state,
				bond_info->bond_mgr);
		if (IS_ERR(bond_state))
			return PTR_ERR(bond_state);

		/* clear single state */
		if (old_conn_state->best_encoder ==
				dp_display->bridge->base.encoder) {
			bond_state->connector_mask &=
				~(1 << bond_info->bond_idx);
			return 0;
		}

		/* clear bond state */
		bond_bridge = to_dp_bond_bridge(
				old_conn_state->best_encoder->bridge);
		bond_state->connector[bond_bridge->type] = NULL;
		bond_state->bond_mask[bond_bridge->type] = 0;
		bond_state->connector_mask &= ~bond_bridge->bond_mask;
	}

	return 0;
}
