// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018  Nexell Co., Ltd.
// Youngbok, Park <ybpark@nexell.co.kr>

#include <asm/mach/arch.h>

static const char * const nexell_dt_compat[] = {
	"nexell,nxp3220",
	NULL
};

/*------------------------------------------------------------------------------
 * Maintainer: Nexell Co., Ltd.
 */
#ifdef CONFIG_ARCH_NXP3220_COMMON
DT_MACHINE_START(NEXELL_DT, "nxp322x (Device Tree Support)")
#else
DT_MACHINE_START(NEXELL_DT, "NEXELL (Device Tree Support)")
#endif
	.dt_compat	= nexell_dt_compat,
MACHINE_END
