// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018  Nexell Co., Ltd.
 * Author: JungHyun, Kim <jhkim@nexell.co.kr>
 */

#include <linux/module.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/arm-smccc.h>

#define NEXELL_SMC_FN_BASE	0x82000000
#define NEXELL_SMC_FN(n)	(NEXELL_SMC_FN_BASE + (n))
#define NEXELL_SMC_FN_WRITE	NEXELL_SMC_FN(0x0)
#define NEXELL_SMC_FN_READ	NEXELL_SMC_FN(0x1)

struct nx_secure {
	struct device *dev;
	struct device_node *np;
	struct regmap *regmap;
	phys_addr_t base;
	int size;
	struct clk *clk;
	struct regmap_config config;
};

static unsigned long sercure_rw(unsigned long function_id,
				unsigned long reg, unsigned long val)
{
	struct arm_smccc_res res;

	arm_smccc_smc(function_id, reg, val, 0, 0, 0, 0, 0, &res);
	return res.a0;
}

static int nx_secure_read(void *context, unsigned int reg, unsigned int *val)
{
	struct nx_secure *sec = context;
	struct regmap_config *config = &sec->config;
	int ret;

	if (reg > (sec->base + sec->size) || sec->base < reg) {
		dev_err(sec->dev,
			"request 0x%x over to range 0x%x~0x%x\n",
			reg, sec->base, sec->base + sec->size);
		return -EINVAL;
	}

	if (!IS_ERR(sec->clk)) {
		ret = clk_prepare_enable(sec->clk);
		if (ret < 0) {
			dev_err(sec->dev,
				"cell %s failed to enable sec clk\n",
				config->name);
			return ret;
		}
	}

	*val = sercure_rw(NEXELL_SMC_FN_READ, (u32)(sec->base + reg), 0);

	if (!IS_ERR(sec->clk))
		clk_disable_unprepare(sec->clk);

	dev_dbg(sec->dev,
		"reg:0x%x, val:0x%x\n", (u32)(sec->base + reg), *val);

	return 0;
}

static int nx_secure_write(void *context, unsigned int reg, unsigned int val)
{
	struct nx_secure *sec = context;
	struct regmap_config *config = &sec->config;
	int ret;

	if (reg > (sec->base + sec->size) || sec->base < reg) {
		dev_err(sec->dev,
			"request 0x%x over to range 0x%x~0x%x\n",
			reg, sec->base, sec->base + sec->size);
		return -EINVAL;
	}

	if (!IS_ERR(sec->clk)) {
		ret = clk_prepare_enable(sec->clk);
		if (ret < 0) {
			dev_err(sec->dev,
				"cell %s failed to enable sec clk\n",
				config->name);
			return ret;
		}
	}

	sercure_rw(NEXELL_SMC_FN_WRITE, (u32)(sec->base + reg), val);

	if (!IS_ERR(sec->clk))
		clk_disable_unprepare(sec->clk);

	dev_dbg(sec->dev,
		"reg:0x%x, val:0x%x\n", (u32)(sec->base + reg), val);

	return 0;
}

static int nx_secure_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev_of_node(dev);
	struct resource *res;
	struct nx_secure *sec;
	struct regmap_config *config;
	const char *compatible;
	int cplen;

	sec = devm_kzalloc(dev, sizeof(*sec), GFP_KERNEL);
	if (!sec)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENOMEM;

	sec->clk = devm_clk_get(dev, NULL);
	sec->dev = dev;
	sec->base = (phys_addr_t)res->start;
	sec->size = resource_size(res);

	compatible = of_get_property(node, "compatible", &cplen);

	config = &sec->config;
	config->name = compatible;
	config->reg_bits = 32;
	config->val_bits = 32;
	config->reg_stride = 4;
	config->cache_type = REGCACHE_NONE;
	config->reg_read = nx_secure_read;
	config->reg_write = nx_secure_write;

	sec->regmap = devm_regmap_init(dev, NULL, sec, &sec->config);
	if (IS_ERR(sec->regmap)) {
		dev_err(dev, "regmap init failed\n");
		return PTR_ERR(sec->regmap);
	}

	platform_set_drvdata(pdev, sec);

	dev_info(dev, "regmap:0x%x, 0x%x\n", sec->base, sec->size);

	/* populate for subnode */
	return devm_of_platform_populate(dev);
}

static const struct of_device_id nx_secure_match[] = {
	{ .compatible = "nexell,secure-regmap", },
	{ },
};
MODULE_DEVICE_TABLE(of, nx_secure_match);

static struct platform_driver nx_secure_driver = {
	.probe = nx_secure_probe,
	.driver = {
		.name = "nexell-secure-regmap",
		.of_match_table = nx_secure_match,
	},
};

static int __init nx_secure_init(void)
{
	return platform_driver_register(&nx_secure_driver);
}

static void __exit nx_secure_exit(void)
{
	platform_driver_unregister(&nx_secure_driver);
}
arch_initcall(nx_secure_init);
module_exit(nx_secure_exit);

MODULE_AUTHOR("JungHyun <jhkim@nexell.co.kr>");
MODULE_DESCRIPTION("Nexell System secure register driver");
MODULE_LICENSE("GPL v2");
