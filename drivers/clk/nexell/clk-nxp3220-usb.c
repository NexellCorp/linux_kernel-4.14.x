// SPDX-License-Identifier: GPL-2.0+
/*
 * Nexell nxp3220 SoC Clock CMU-USB driver
 * Copyright (c) 2018 Chanho Park <chanho61.park@samsung.com>
 */

#include <linux/of_address.h>
#include "clk-nexell.h"
#include <dt-bindings/clock/nxp3220-clk.h>

#define USB_AHB			0x0200

#define DIV_USB(_id, cname, pname, o)	\
	__DIV(_id, NULL, cname, pname, o, 0, 8, 0, 0)

static const struct nexell_div_clock usb_div_clks[] __initconst = {
	DIV_USB(CLK_USB_AHB, "div_usb_ahb", "src_usb0_ahb",
		USB_AHB + 0x60),
};

static const struct nexell_gate_clock usb_gate_clks[] __initconst = {
	GATE(CLK_USB_AHB, "usb_ahb", "div_usb_ahb",
	     USB_AHB + 0x10, 0, 0, 0),
	GATE(CLK_USB_BLK_BIST, "usb_blk_bist", "div_usb_ahb",
	     USB_AHB + 0x10, 1, 0, 0),
	GATE(CLK_USB_SYSREG_APB, "usb_sysreg_apb", "div_usb_ahb",
	     USB_AHB + 0x10, 2, 0, 0),
	GATE(CLK_USB_USB20HOST, "usb20host", "div_usb_ahb",
	     USB_AHB + 0x10, 3, 0, 0),
	GATE(CLK_USB_USB20OTG, "usb20otg", "div_usb_ahb",
	     USB_AHB + 0x10, 4, 0, 0),
};

static void __init nxp3220_cmu_usb_init(struct device_node *np)
{
	void __iomem *reg;
	struct nexell_clk_data *ctx;

	reg = of_iomap(np, 0);
	if (!reg) {
		pr_err("%s: Failed to get base ausbess\n", __func__);
		return;
	}

	ctx = nexell_clk_init(reg, CLK_CPU_NR);
	if (!ctx) {
		pr_err("%s: Failed to initialize clock data\n", __func__);
		return;
	}

	nexell_clk_register_div(ctx, usb_div_clks, ARRAY_SIZE(usb_div_clks));
	nexell_clk_register_gate(ctx, usb_gate_clks,
				 ARRAY_SIZE(usb_gate_clks));

	if (of_clk_add_provider(np, of_clk_src_onecell_get, &ctx->clk_data))
		pr_err("%s: failed to add clock provider\n", __func__);
}
CLK_OF_DECLARE(nxp3220_cmu_usb, "nexell,nxp3220-cmu-usb",
	       nxp3220_cmu_usb_init);
