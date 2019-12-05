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
 *
 */

#ifndef __MDSS_EDP_PLL_7NM_H
#define __MDSS_EDP_PLL_7NM_H

#define EDP_VCO_HSCLK_RATE_1620MHZDIV1000	1620000UL
#define EDP_VCO_HSCLK_RATE_2160MHZDIV1000	2160000UL
#define EDP_VCO_HSCLK_RATE_2430MHZDIV1000	2430000UL
#define EDP_VCO_HSCLK_RATE_2700MHZDIV1000	2700000UL
#define EDP_VCO_HSCLK_RATE_3240MHZDIV1000	3240000UL
#define EDP_VCO_HSCLK_RATE_4320MHZDIV1000	4320000UL
#define EDP_VCO_HSCLK_RATE_5400MHZDIV1000	5400000UL
#define EDP_VCO_HSCLK_RATE_5940MHZDIV1000	5940000UL
#define EDP_VCO_HSCLK_RATE_8100MHZDIV1000	8100000UL

struct edp_pll_db_7nm {
	struct mdss_pll_resources *pll;

	/* lane and orientation settings */
	u8 lane_cnt;
	u8 orientation;

	/* COM PHY settings */
	u32 hsclk_sel;
	u32 dec_start_mode0;
	u32 div_frac_start1_mode0;
	u32 div_frac_start2_mode0;
	u32 div_frac_start3_mode0;
	u32 integloop_gain0_mode0;
	u32 integloop_gain1_mode0;
	u32 vco_tune_map;
	u32 lock_cmp1_mode0;
	u32 lock_cmp2_mode0;
	u32 vco_tune1_mode0;
	u32 vco_tune2_mode0;
	u32 lock_cmp_en;
	u32 cmn_config;
	u32 txn_tran_drv_emp_en;
	u32 ssc_adj_per1;
	u32 ssc_per1;
	u32 ssc_per2;
	u32 ssc_step_size1_mode0;
	u32 ssc_step_size2_mode0;

	/* PHY vco divider */
	u32 phy_vco_div;
};

int edp_vco_set_rate_7nm(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate);
unsigned long edp_vco_recalc_rate_7nm(struct clk_hw *hw,
				unsigned long parent_rate);
long edp_vco_round_rate_7nm(struct clk_hw *hw, unsigned long rate,
				unsigned long *parent_rate);
int edp_vco_prepare_7nm(struct clk_hw *hw);
void edp_vco_unprepare_7nm(struct clk_hw *hw);
int edp_mux_set_parent_7nm(void *context,
				unsigned int reg, unsigned int val);
int edp_mux_get_parent_7nm(void *context,
				unsigned int reg, unsigned int *val);
#endif /* __MDSS_EDP_PLL_7NM_H */
