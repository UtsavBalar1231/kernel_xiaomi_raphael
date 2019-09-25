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


#include <linux/init.h>
#include <linux/async.h>
#include <linux/early_userspace.h>

const char * const early_dev_nodes[] = {
	"/soc/qcom,gdsc@0x177004",
	"/soc/qcom,rpmhclk",
	"/soc/qcom,gcc",
	"/soc/ad-hoc-bus",
	"/soc/ufsice@1d90000",
	"/soc/ufsphy_mem@1d87000",
	"/soc/ufshc@1d84000",
	"/soc/mailbox@18220000",
	"/soc/qcom,cmd-db@c3f000c",
	"/soc/rpmh-regulator-ldoa10",
	"/soc/rpmh-regulator-ldoc5",
	"/soc/rpmh-regulator-ldoc8",
	"/soc/rpmh-regulator-smpa4",
	"/soc/pinctrl@03000000",
	"/soc/rpmh-regulator-ldoa5",
	"/soc/rpmh-regulator-cxlvl",
	"/soc/rpmh-regulator-mxlvl",
	"/soc/qcom,gdsc@0xad07004",
	"/soc/qcom,gdsc@0xad08004",
	"/soc/qcom,gdsc@0xad09004",
	"/soc/qcom,gdsc@0xad0a004",
	"/soc/qcom,gdsc@0xad0b004",
	"/soc/qcom,gdsc@0xad0c1bc",
	"/soc/qcom,gdsc@0xab00814",
	"/soc/qcom,gdsc@0xab00874",
	"/soc/qcom,gdsc@0xab008b4",
	NULL
};

bool is_early_userspace;

static int __init early_userspace(char *p)
{
	is_early_userspace = true;
	return 0;
}
early_param("early_userspace", early_userspace);

static void __init early_rootfs_init(void *data, async_cookie_t cookie)
{
	int ret;

	#ifdef CONFIG_COMMON_CLK_QCOM
	ret = early_gdsc_init();
	if (ret)
		pr_err("%s: gdsc_init fails with %d\n", __func__, ret);
	#endif
	#ifdef CONFIG_MSM_CLK_RPMH
	ret = early_clk_rpmh_init();
	if (ret)
		pr_err("%s: clk_rpmh_init fails with %d\n", __func__, ret);
	#endif
	#ifdef CONFIG_MSM_GCC_SM8150
	ret = early_gcc_sm8150_init();
	if (ret)
		pr_err("%s: gcc_sm8150_init fails with %d\n", __func__, ret);
	#endif
	#ifdef CONFIG_QCOM_BUS_CONFIG_RPMH
	ret = early_msm_bus_device_init_driver();
	if (ret)
		pr_err("%s: msm_bus_device_init_driver fails with %d\n",
			__func__, ret);
	#endif
	ret = early_init_bio();
	if (ret)
		pr_err("%s: init_bio fails with %d\n", __func__, ret);
	ret = early_blk_settings_init();
	if (ret)
		pr_err("%s: blk_settings_init fails with %d\n", __func__, ret);
	ret = early_blk_ioc_init();
	if (ret)
		pr_err("%s: blk_ioc_init fails with %d\n", __func__, ret);
	ret = early_blk_softirq_init();
	if (ret)
		pr_err("%s: blk_softirq_init fails with %d\n", __func__, ret);
	ret = early_genhd_device_init();
	if (ret)
		pr_err("%s: genhd_device_init fails with %d\n", __func__, ret);
	#ifdef CONFIG_SCSI
	ret = early_init_scsi();
	if (ret)
		pr_err("%s: init_scsi fails with %d\n", __func__, ret);
	#endif
	#ifdef CONFIG_PFK
	ret = early_pfk_init();
	if (ret)
		pr_err("%s: pfk_init fails with %d\n", __func__, ret);
	#endif
	#ifdef CONFIG_BLK_DEV_BSG
	ret = early_bsg_init();
	if (ret)
		pr_err("%s: bsg_init fails with %d\n", __func__, ret);
	#endif
	#ifdef CONFIG_IOSCHED_CFQ
	ret = early_cfq_init();
	if (ret)
		pr_err("%s: cfq_init fails with %d\n", __func__, ret);
	#endif
	#ifdef CONFIG_SG_POOL
	ret = early_sg_pool_init();
	if (ret)
		pr_err("%s: sg_pool_init fails with %d\n", __func__, ret);
	#endif
	#ifdef CONFIG_GENERIC_PHY
	ret = early_phy_core_init();
	if (ret)
		pr_err("%s: phy_core_init fails with %d\n", __func__, ret);
	#endif
	#ifdef CONFIG_PHY_QCOM_UFS
	ret = early_ufs_qcom_phy_qmp_v4_driver_init();
	if (ret)
		pr_err("%s: ufs_qcom_phy_qmp_v4_driver_init fails with %d\n",
			__func__, ret);
	#endif
	#ifdef CONFIG_CRYPTO_DEV_QCOM_ICE
	ret = early_qcom_ice_driver_init();
	if (ret)
		pr_err("%s: qcom_ice_driver_init fails with %d\n",
			__func__, ret);
	#endif
	#ifdef CONFIG_BLK_DEV_SD
	ret = early_init_sd();
	if (ret)
		pr_err("%s: init_sd fails with %d\n", __func__, ret);
	#endif
	#ifdef CONFIG_PM_DEVFREQ
	ret = early_devfreq_init();
	if (ret)
		pr_err("%s: devfreq_init fails with %d\n", __func__, ret);
	#endif
	#ifdef CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND
	ret = early_devfreq_simple_ondemand_init();
	if (ret)
		pr_err("%s: devfreq_simple_ondemand_init fails with %d\n",
			__func__, ret);
	#endif
	#ifdef CONFIG_SCSI_UFS_QCOM
	ret = early_ufs_qcom_driver_init();
	if (ret)
		pr_err("%s: ufs_qcom_driver_init fails with %d\n",
			__func__, ret);
	#endif
	#ifdef CONFIG_EXT4_FS
	ret = early_ext4_init_fs();
	if (ret)
		pr_err("%s: ext4_init_fs fails with %d\n", __func__, ret);
	#endif
	#ifdef CONFIG_BLK_DEV_INITRD
	ret = early_populate_rootfs();
	if (ret)
		pr_err("%s: populate_rootfs fails with %d\n", __func__, ret);
	#endif
	#ifdef CONFIG_SOC_BUS
	ret = early_socinfo_init();
	if (ret)
		pr_err("%s: socinfo_init fails with %d\n", __func__, ret);
	#endif
	#ifdef CONFIG_MSM_BOOT_TIME_MARKER
	ret = early_init_bootkpi();
	if (ret)
		pr_err("%s: init_bootkpi fails with %d\n", __func__, ret);
	#endif
	ret = early_crc32c_mod_init();
	if (ret)
		pr_err("%s: crc32c_mod_init fails with %d\n", __func__, ret);
	#ifdef CONFIG_JBD2
	ret = early_journal_init();
	if (ret)
		pr_err("%s: journal_init fails with %d\n", __func__, ret);
	#endif
	early_prepare_namespace();
}

void __ref early_rootfs_init_async(void)
{
	async_schedule(early_rootfs_init, NULL);
}
