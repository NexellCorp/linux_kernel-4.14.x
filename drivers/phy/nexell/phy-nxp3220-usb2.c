// SPDX-License-Identifier: GPL-2.0+
/*
 * NXP3220 USB2 PHY driver
 * Copyright (c) 2018 JungHyun Kim <jhkim@nexell.co.kr>
 */

static int nx_otg_phy_power_on(struct nx_usb2_phy *p)
{
	struct nx_usb2_phy_pdata *pdata = p->pdata;
	void __iomem *base = pdata->base;

	dev_dbg(pdata->dev, "power on '%s'\n", p->label);

	/*
	 * Must be enabled 'adb400 blk usb cfg' and 'data adb'
	 * 'adb400 blk usb cfg' and 'data adb' is shared EHCI
	 */

	/*
	 * PHY POR release
	 * SYSREG_USB_USB20PHY_OTG0_i_POR/SYSREG_USB_USB20PHY_OTG0_i_POR_ENB
	 */
	writel(readl(base + 0x80) & ~(1 << 4), base + 0x80);
	writel(readl(base + 0x80) | (1 << 3), base + 0x80);

	udelay(50);

	/*
	 * PHY reset release in OTG LINK
	 * SYSREG_USB_OTG_i_nUtmiResetSync
	 */
	writel(readl(base + 0x60) | (1 << 8), base + 0x60);

	udelay(1);

	/*
	 * BUS reset release in OTG LINK
	 * SYSREG_USB_OTG_i_nResetSync
	 */
	writel(readl(base + 0x60) | (1 << 7), base + 0x60);

	udelay(1);

	return 0;
}

static int nx_otg_phy_power_off(struct nx_usb2_phy *p)
{
	struct nx_usb2_phy_pdata *pdata = p->pdata;
	void __iomem *base = pdata->base;

	dev_dbg(pdata->dev, "power off '%s'\n", p->label);

	/*
	 * PHY reset in OTG LINK
	 * SYSREG_USB_OTG_i_nUtmiResetSync
	 */
	writel(readl(base + 0x60) & ~(1 << 8), base + 0x60);

	/*
	 * BUS reset in OTG LINK
	 * SYSREG_USB_OTG_i_nResetSync
	 */
	writel(readl(base + 0x60) & ~(1 << 7), base + 0x60);

	/*
	 * PHY POR
	 * SYSREG_USB_USB20PHY_OTG0_i_POR/SYSREG_USB_USB20PHY_OTG0_i_POR_ENB
	 */
	writel(readl(base + 0x80) | (1 << 4), base + 0x80);

	/*
	 * Don't disable 'adb400 blk usb cfg' and 'data adb'
	 * 'adb400 blk usb cfg' and 'data adb' is shared EHCI
	 */

	return 0;
}

static struct nx_usb2_phy nxp3220_usb2_phys[] = {
	{
		.label = "dwc2otg",
		.vbus_gpio = "otg,vbus-gpio",
		.bus_width = 16,
		.power_on = nx_otg_phy_power_on,
		.power_off = nx_otg_phy_power_off,
	},
};

static const struct nx_usb2_phy_config nxp3220_usb2_phy_cfg = {
	.phys = nxp3220_usb2_phys,
	.num_phys = ARRAY_SIZE(nxp3220_usb2_phys),
};
