// SPDX-License-Identifier: GPL-2.0+
/*
 * Nexell nxp3220 SoC Clock CMU-CPU driver
 * Copyright (c) 2018 Chanho Park <chanho61.park@samsung.com>
 */

#include <linux/of_address.h>
#include "clk-nexell.h"
#include <dt-bindings/clock/nxp3220-clk.h>

#define CPU_CPU0_ARM			0x0200
#define PLL_CPU_DIV0			0x0400
#define HPM_CPU0			0x0600

#define DIV_CPU(_id, cname, pname, o)	\
	__DIV(_id, NULL, cname, pname, o, 0, 8, 0, 0)

/* FIXME: resolve div_sys_cpu_backup0 dependency chain */
PNAME(cpu_mux_p)		= { "pll_cpu" };

static const struct nexell_mux_clock cpu_mux_clks[] __initconst = {
	MUX(CLK_MUX_CPU_ARM, "mux_cpu_arm", cpu_mux_p, CPU_CPU0_ARM, 0, 4),
	MUX(CLK_MUX_CPU_HPM, "mux_cpu_hpm", cpu_mux_p, HPM_CPU0, 0, 4),
};

static const struct nexell_div_clock cpu_div_clks[] __initconst = {
	DIV_CPU(CLK_CPU_DIV_ARM, "div_cpu_arm", "mux_cpu_arm",
		CPU_CPU0_ARM + 0x60),
	DIV_CPU(CLK_CPU_DIV_AXI, "div_cpu_axi", "div_cpu_arm",
		CPU_CPU0_ARM + 0x64),
	DIV_CPU(CLK_CPU_DIV_ATCLK, "div_cpu_atclk", "div_cpu_arm",
		CPU_CPU0_ARM + 0x68),
	DIV_CPU(CLK_CPU_DIV_CNTCLK, "div_cpu_cntclk", "div_cpu_arm",
		CPU_CPU0_ARM + 0x6c),
	DIV_CPU(CLK_CPU_DIV_TSCLK, "div_cpu_tsclk", "div_cpu_arm",
		CPU_CPU0_ARM + 0x70),
	DIV_CPU(CLK_CPU_DIV_DBGAPB, "div_cpu_dbgapb", "div_cpu_arm",
		CPU_CPU0_ARM + 0x74),
	DIV_CPU(CLK_CPU_DIV_APB, "div_cpu_apb", "div_cpu_arm",
		CPU_CPU0_ARM + 0x78),
	DIV_CPU(CLK_CPU_DIV_PLL, "div_cpu_pll", "pll_cpu",
		PLL_CPU_DIV0 + 0x60),
	DIV_CPU(CLK_CPU_DIV_HPM, "div_cpu_hpm", "mux_cpu_hpm",
		HPM_CPU0 + 0x60),
};

static const struct nexell_gate_clock cpu_gate_clks[] __initconst = {
	GATE(CLK_CPU_ARM, "cpu_arm", "div_cpu_arm",
	     CPU_CPU0_ARM + 0x10, 0, 0, 0),
	GATE(CLK_CPU_BLK_BIST, "cpu_blk_bist", "div_cpu_arm",
	     CPU_CPU0_ARM + 0x10, 1, 0, 0),
	GATE(CLK_CPU_AXI, "cpu_axi", "div_cpu_axi",
	     CPU_CPU0_ARM + 0x10, 2, 0, 0),
	GATE(CLK_CPU_AXIM, "cpu_axim", "div_cpu_axi",
	     CPU_CPU0_ARM + 0x10, 3, 0, 0),
	GATE(CLK_CPU_ATCLK, "cpu_atclk", "div_cpu_atclk",
	     CPU_CPU0_ARM + 0x10, 4, 0, 0),
	GATE(CLK_CPU_CNTCLK, "cpu_cntclk", "div_cpu_cntclk",
	     CPU_CPU0_ARM + 0x10, 5, 0, 0),
	GATE(CLK_CPU_TSCLK, "cpu_tsclk", "div_cpu_tsclk",
	     CPU_CPU0_ARM + 0x10, 6, 0, 0),
	GATE(CLK_CPU_DBGAPB, "cpu_dbgapb", "div_cpu_dbgapb",
	     CPU_CPU0_ARM + 0x10, 7, 0, 0),
	GATE(CLK_CPU_APB, "cpu_apb", "div_cpu_apb",
	     CPU_CPU0_ARM + 0x10, 8, 0, 0),
	GATE(CLK_CPU_SYSREG_APB, "cpu_sysreg_apb", "div_cpu_apb",
	     CPU_CPU0_ARM + 0x10, 9, 0, 0),
	GATE(CLK_CPU_AXIM_APB, "cpu_axim_apb", "div_cpu_apb",
	     CPU_CPU0_ARM + 0x10, 10, 0, 0),

	GATE(CLK_CPU_PLL_DIV, "cpu_pll_div", "div_cpu_pll",
	     PLL_CPU_DIV0 + 0x10, 0, 0, 0),
	GATE(CLK_CPU_HPM, "cpu_hpm", "div_cpu_hpm",
	     HPM_CPU0 + 0x10, 0, 0, 0),
};

static void __init nxp3220_cmu_cpu_init(struct device_node *np)
{
	void __iomem *reg;
	struct nexell_clk_data *ctx;

	reg = of_iomap(np, 0);
	if (!reg) {
		pr_err("%s: Failed to get base address\n", __func__);
		return;
	}

	ctx = nexell_clk_init(reg, CLK_CPU_NR);
	if (!ctx) {
		pr_err("%s: Failed to initialize clock data\n", __func__);
		return;
	}

	nexell_clk_register_mux(ctx, cpu_mux_clks, ARRAY_SIZE(cpu_mux_clks));
	nexell_clk_register_div(ctx, cpu_div_clks, ARRAY_SIZE(cpu_div_clks));
	nexell_clk_register_gate(ctx, cpu_gate_clks,
				 ARRAY_SIZE(cpu_gate_clks));

	if (of_clk_add_provider(np, of_clk_src_onecell_get, &ctx->clk_data))
		pr_err("%s: failed to add clock provider\n", __func__);
}
CLK_OF_DECLARE(nxp3220_cmu_cpu, "nexell,nxp3220-cmu-cpu",
	       nxp3220_cmu_cpu_init);
