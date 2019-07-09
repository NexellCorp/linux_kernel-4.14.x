// SPDX-License-Identifier: GPL-2.0+
/*
 * Nexell AXIM QoS driver
 * Copyright (c) 2018 JungHyun Kim <jhkim@nexell.co.kr>
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#define AXIM_AXQOS_SHIFT	(8)
#define AXIM_AXQOS_MASK		(0x1 << AXIM_AXQOS_SHIFT)
#define AXIM_AWQOS_SHIFT	(4)
#define AXIM_AWQOS_MASK		(0xf << AXIM_AWQOS_SHIFT)
#define AXIM_ARQOS_SHIFT	(0)
#define AXIM_ARQOS_MASK		(0xf << AXIM_ARQOS_SHIFT)

struct axim_busqos {
	struct device *dev;
	struct regmap *regmap;
	bool axqos_enb;
	u32 awqos;
	u32 arqos;
};

static int nx_qos_setup_resource(struct device *dev,
			       struct axim_busqos *qos)
{
	struct device_node *np = dev->of_node;
	struct regmap *regmap;

	regmap = syscon_node_to_regmap(np);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	qos->regmap = regmap;
	qos->dev = dev;

	return 0;
}

static int nx_qos_parse_dt(struct device *dev,
			       struct axim_busqos *qos)
{
	struct device_node *np = dev->of_node;

	qos->axqos_enb = of_property_read_bool(np, "axqos-enb");
	of_property_read_u32(np, "awqos-value", &qos->awqos);
	of_property_read_u32(np, "arqos-value", &qos->arqos);

	dev_info(dev,
		"QoS:%s, aw:0x%x, ar:0x%x\n",
		qos->axqos_enb ? "ON " : "OFF", qos->awqos, qos->arqos);

	return 0;
}

static void nx_qos_setup_bus(struct axim_busqos *qos)
{
	struct regmap *regmap = qos->regmap;
	u32 mask, val;

	mask = AXIM_AXQOS_MASK | AXIM_AWQOS_MASK | AXIM_ARQOS_MASK;
	val = ((qos->axqos_enb ? 1 : 0) << AXIM_AXQOS_SHIFT) |
		((qos->awqos << AXIM_AWQOS_SHIFT) & AXIM_AWQOS_MASK) |
		((qos->arqos << AXIM_ARQOS_SHIFT) & AXIM_ARQOS_MASK);

	dev_dbg(qos->dev,
		"setup use-axqos:%s, awqos:0x%x, arqos:0x%x\n",
		qos->axqos_enb ? "ON " : "OFF",
		qos->awqos, qos->arqos);

	regmap_update_bits(regmap, 0x0, mask, val);
}

/* platform driver interface */
static int nx_qos_probe(struct platform_device *pdev)
{
	struct axim_busqos *qos;
	struct device *dev = &pdev->dev;
	int ret;

	qos = devm_kzalloc(dev, sizeof(*qos), GFP_KERNEL);
	if (!qos)
		return -ENOMEM;

	ret = nx_qos_setup_resource(dev, qos);
	if (ret)
		return ret;

	nx_qos_parse_dt(dev, qos);
	nx_qos_setup_bus(qos);

	platform_set_drvdata(pdev, qos);

	return 0;
}

static int nx_qos_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int nx_qos_suspend(struct device *dev)
{
	return 0;
}

static int nx_qos_resume(struct device *dev)
{
	struct axim_busqos *qos = dev_get_drvdata(dev);

	if (qos)
		nx_qos_setup_bus(qos);

	return 0;
}

static const struct dev_pm_ops nx_qos_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(nx_qos_suspend, nx_qos_resume)
};
#endif

static const struct of_device_id nx_qos_match[] = {
	{ .compatible = "nexell,nxp3220-axim-qos" },
	{ },
};

static struct platform_driver nx_qos_driver = {
	.probe = nx_qos_probe,
	.remove = nx_qos_remove,
	.driver = {
		.name = "nx-axim-qos",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
		.pm = &nx_qos_pm_ops,
#endif
		.of_match_table = nx_qos_match,
	},
};

static int __init nx_qos_init(void)
{
	return platform_driver_register(&nx_qos_driver);
}
arch_initcall(nx_qos_init);

MODULE_AUTHOR("jhkim <jhkim@nexell.co.kr>");
MODULE_DESCRIPTION("AXIM BUS QoS driver for the Nexell");
MODULE_LICENSE("GPL v2");
