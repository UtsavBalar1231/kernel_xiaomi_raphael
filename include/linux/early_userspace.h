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

#ifndef __EARLY_USERSPACE_H__
#define __EARLY_USERSPACE_H__

extern bool is_early_userspace;
extern const char * const early_dev_nodes[];

int __init early_init_bio(void);
int __init early_blk_ioc_init(void);
int __init early_blk_settings_init(void);
int __init early_blk_softirq_init(void);
#ifdef CONFIG_BLK_DEV_BSG
int __init early_bsg_init(void);
#endif
#ifdef CONFIG_IOSCHED_CFQ
int __init early_cfq_init(void);
#endif
int __init early_genhd_device_init(void);
int __init early_crc32c_mod_init(void);
#ifdef CONFIG_MSM_CLK_RPMH
int __init early_clk_rpmh_init(void);
#endif
#ifdef CONFIG_MSM_GCC_SM8150
int __init early_gcc_sm8150_init(void);
#endif
#ifdef CONFIG_COMMON_CLK_QCOM
int __init early_gdsc_init(void);
#endif
#ifdef CONFIG_CRYPTO_DEV_QCOM_ICE
int __init early_qcom_ice_driver_init(void);
#endif
#ifdef CONFIG_PM_DEVFREQ
int __init early_devfreq_init(void);
#endif
#ifdef CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND
int __init early_devfreq_simple_ondemand_init(void);
#endif
#ifdef CONFIG_GENERIC_PHY
int __init early_phy_core_init(void);
#endif
#ifdef CONFIG_PHY_QCOM_UFS
int __init early_ufs_qcom_phy_qmp_v4_driver_init(void);
#endif
#ifdef CONFIG_SCSI
int __init early_init_scsi(void);
#endif
#ifdef CONFIG_BLK_DEV_SD
int __init early_init_sd(void);
#endif
#ifdef CONFIG_SCSI_UFS_QCOM
int __init early_ufs_qcom_driver_init(void);
#endif
#ifdef CONFIG_MSM_BOOT_TIME_MARKER
int __init early_init_bootkpi(void);
#endif
#ifdef CONFIG_QCOM_BUS_CONFIG_RPMH
int __init early_msm_bus_device_init_driver(void);
#endif
#ifdef CONFIG_SOC_BUS
int __init early_socinfo_init(void);
#endif
#ifdef CONFIG_EXT4_FS
int __init early_ext4_init_fs(void);
#endif
#ifdef CONFIG_JBD2
int __init early_journal_init(void);
#endif
#ifdef CONFIG_BLK_DEV_INITRD
int __init early_populate_rootfs(void);
#endif
#ifdef CONFIG_SG_POOL
int __init early_sg_pool_init(void);
#endif
#ifdef CONFIG_PFK
int __init early_pfk_init(void);
#endif
void __init early_prepare_namespace(void);
void early_rootfs_init_async(void);

#endif /* __EARLY_USERSPACE_H__ */
