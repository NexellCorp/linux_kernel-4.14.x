// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018  Nexell Co., Ltd.
 * Author: JungHyun, Kim <jhkim@nexell.co.kr>
 */
#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/nvmem-provider.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#define EFUSE_WORD_SIZE		4
#define EFUSE_STRIDE		4

struct nx_efuse_priv {
	struct device *dev;
	const char *name;
	int id;
	struct regmap *regmap;
	phys_addr_t reg;
	int size;
	struct nvmem_config config;
};

static int nx_efuse_read(void *context, unsigned int offset,
			 void *val, size_t bytes)
{
	struct nx_efuse_priv *efuse = context;
	struct nvmem_config *config = &efuse->config;
	unsigned int offs = efuse->reg + offset;
	u32 *buf = val;
	int size, i, ret;

	if (!IS_ALIGNED(offset, EFUSE_WORD_SIZE) ||
	    !IS_ALIGNED(bytes, EFUSE_WORD_SIZE)) {
		dev_err(efuse->dev, "cell %s unaligned to word size %d\n",
			config->name, config->word_size);
		return -EINVAL;
	}

	if (offset + bytes > efuse->size) {
		dev_err(efuse->dev,
			"cell %s request 0x%x:0x%x over to size %d\n",
			config->name, offset, offset + bytes, efuse->size);
		return -EINVAL;
	}

	size = bytes > efuse->size ? efuse->size : bytes;

	for (i = 0; i < (size / EFUSE_WORD_SIZE); i++) {
		ret = regmap_read(efuse->regmap,
				  offs + (i * EFUSE_WORD_SIZE), &buf[i]);
		if (ret) {
			dev_err(efuse->dev,
				"Error regmap[%d] read:0x%x, ret:%d\n",
				i, offs + (i * EFUSE_WORD_SIZE), ret);
			return 0;
		}
	}

	return size;
}

static int nx_efuse_probe(struct platform_device *pdev)
{
	struct nvmem_device *nvmem;
	struct nvmem_config *config;
	struct nx_efuse_priv *efuse;
	struct device *dev = &pdev->dev;
	struct resource *res;

	efuse = devm_kzalloc(dev, sizeof(struct nx_efuse_priv),
			     GFP_KERNEL);
	if (!efuse)
		return -ENOMEM;

	efuse->regmap = dev_get_regmap(dev->parent, NULL);
	if (!efuse->regmap) {
		dev_err(&pdev->dev, "Parent regmap unavailable.\n");
		return -ENXIO;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	efuse->name = of_get_property(dev_of_node(dev), "cell-name", NULL);
	of_property_read_s32(dev_of_node(dev), "cell-id", &efuse->id);

	efuse->dev = dev;
	efuse->reg = res->start;
	efuse->size = resource_size(res);

	config = &efuse->config;
	config->name = efuse->name;
	config->id = efuse->id;
	config->priv = efuse;
	config->dev = efuse->dev;
	config->owner = THIS_MODULE;
	config->read_only = true;
	config->reg_read = &nx_efuse_read;
	config->size = efuse->size;
	config->word_size = EFUSE_WORD_SIZE;
	config->stride = EFUSE_STRIDE;

	nvmem = nvmem_register(config);
	if (IS_ERR(nvmem))
		return PTR_ERR(nvmem);

	platform_set_drvdata(pdev, nvmem);

	dev_dbg(dev, "nvmem %s 0x%x:0x%x\n",
		efuse->name, efuse->reg, efuse->size);

	return 0;
}

static int nx_efuse_remove(struct platform_device *pdev)
{
	struct nvmem_device *nvmem = platform_get_drvdata(pdev);

	return nvmem_unregister(nvmem);
}

static const struct of_device_id nx_efuse_match[] = {
	{ .compatible = "nexell,nxp3220-efuse", },
	{ },
};
MODULE_DEVICE_TABLE(of, nx_efuse_match);

static struct platform_driver nx_efuse_driver = {
	.probe = nx_efuse_probe,
	.remove = nx_efuse_remove,
	.driver = {
		.name = "nexell-efuse",
		.of_match_table = nx_efuse_match,
	},
};

static int __init nx_efuse_init(void)
{
	return platform_driver_register(&nx_efuse_driver);
}

static void __exit nx_efuse_exit(void)
{
	platform_driver_unregister(&nx_efuse_driver);
}

subsys_initcall(nx_efuse_init);
module_exit(nx_efuse_exit);

MODULE_DESCRIPTION("Nexell efuse driver");
MODULE_LICENSE("GPL v2");
