/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef __SOC_RK3568_GRF_H
#define __SOC_RK3568_GRF_H

#define RK3568_PMUGRF_OS_REG2		0x208
#define RK3568_PMUGRF_OS_REG2_DRAMTYPE_INFO		GENMASK(15, 13)
#define RK3568_PMUGRF_OS_REG2_BW_CH0			GENMASK(3, 2)

#define RK3568_PMUGRF_OS_REG3		0x20c
#define RK3568_PMUGRF_OS_REG3_DRAMTYPE_INFO_V3		GENMASK(13, 12)
#define RK3568_PMUGRF_OS_REG3_SYSREG_VERSION		GENMASK(31, 28)

#endif /* __SOC_RK3568_GRF_H */
