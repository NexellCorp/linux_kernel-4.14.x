// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018  Nexell Co., Ltd.
 * Author: Bon-gyu, KOO <freestyle@nexell.co.kr>
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <linux/mfd/syscon.h>
#include <linux/soc/nexell/sec_io.h>

#define	QOS_SECURE_ACCESS

/* for nxp3220 */
#define QOS_CONTROL_OFFS	0x10C
#define QOS_TARGET_FC_OFFS	0x130
#define QOS_RANGE_OFFS		0x138

#define	QOS_CNTL_AR_FC		20
#define	QOS_CNTL_AW_FC		16
#define	QOS_CNTL_EN_AWAR_OT	7
#define	QOS_CNTL_EN_AR_OT	6
#define	QOS_CNTL_EN_AW_OT	5
#define	QOS_CNTL_EN_AR_FC	4
#define	QOS_CNTL_EN_AW_FC	3
#define	QOS_CNTL_EN_AWAR_RATE	2
#define	QOS_CNTL_EN_AR_RATE	1
#define	QOS_CNTL_EN_AW_RATE	0

#define	QOS_CNTL_MASK_BITS	0x1
#define	QOS_CNTL_MASK_ALL ( \
	(QOS_CNTL_MASK_BITS << QOS_CNTL_AR_FC) | \
	(QOS_CNTL_MASK_BITS << QOS_CNTL_AW_FC) | \
	(QOS_CNTL_MASK_BITS << QOS_CNTL_EN_AWAR_OT) | \
	(QOS_CNTL_MASK_BITS << QOS_CNTL_EN_AR_OT) | \
	(QOS_CNTL_MASK_BITS << QOS_CNTL_EN_AW_OT) | \
	(QOS_CNTL_MASK_BITS << QOS_CNTL_EN_AR_FC) | \
	(QOS_CNTL_MASK_BITS << QOS_CNTL_EN_AW_FC) | \
	(QOS_CNTL_MASK_BITS << QOS_CNTL_EN_AWAR_RATE) | \
	(QOS_CNTL_MASK_BITS << QOS_CNTL_EN_AR_RATE) | \
	(QOS_CNTL_MASK_BITS << QOS_CNTL_EN_AW_RATE) \
	)

#define	QOS_TARGET_FC_AR	16
#define	QOS_TARGET_FC_AW	0

#define	QOS_TARGET_FC_MASK_BITS ((1 << 12) - 1)
#define	QOS_TARGET_FC_MASK_ALL ( \
	(QOS_TARGET_FC_MASK_BITS << QOS_TARGET_FC_AR) | \
	(QOS_TARGET_FC_MASK_BITS << QOS_TARGET_FC_AW) \
	)

#define	QOS_RANGE_AR_MAX	24
#define	QOS_RANGE_AR_MIN	16
#define	QOS_RANGE_AW_MAX	8
#define	QOS_RANGE_AW_MIN	0

#define	QOS_RANGE_BITS ((1 << 4) - 1)
#define	QOS_RANGE_MASK_ALL ( \
	(QOS_RANGE_BITS << QOS_RANGE_AR_MAX) | \
	(QOS_RANGE_BITS << QOS_RANGE_AR_MIN) | \
	(QOS_RANGE_BITS << QOS_RANGE_AW_MAX) | \
	(QOS_RANGE_BITS << QOS_RANGE_AW_MIN) \
	)

struct nic_busqos {
	struct device *dev;
	struct regmap *regmap;
	void *base;
	u32 qos_control;
	u32 target_fc;
	u32 qos_range;
};

/* sys_bus register attribute [3:0] */
enum {
	AR_FC_MODE = 0,
	AW_FC_MODE,
	AR_FC_ENABLE,
	AW_FC_ENABLE,
	AR_FC_TARGET_LATENCY,
	AW_FC_TARGET_LATENCY,
	AR_MAX_QOS,
	AR_MIN_QOS,
	AW_MAX_QOS,
	AW_MIN_QOS,
};

struct nx_debug_qos {
	const char *name;
	u32 type;
	struct nic_busqos *qos;
};

static struct nx_debug_qos nx_debug[] = {
	{ "ar-fc-mode", AR_FC_MODE, },
	{ "aw-fc-mode", AW_FC_MODE, },
	{ "ar-fc-enable", AR_FC_ENABLE, },
	{ "aw-fc-enable", AW_FC_ENABLE, },
	{ "ar-fc-target-latency", AR_FC_TARGET_LATENCY, },
	{ "aw-fc-target-latency", AW_FC_TARGET_LATENCY, },
	{ "ar-max-qos", AR_MAX_QOS, },
	{ "ar-min-qos", AR_MIN_QOS, },
	{ "aw-max-qos", AW_MAX_QOS, },
	{ "aw-min-qos", AW_MIN_QOS, },
};

#define	QOS_DEBUGFS_DIR		"nx_nic_busqos"

static u32 qos_readl(struct nic_busqos *qos, u32 offset)
{
#ifndef QOS_SECURE_ACCESS
	u32 val;

	regmap_read(qos->regmap, offset, &val);
	return val;
#else
	return sec_readl(qos->base + offset);
#endif
}

static void qos_writel(struct nic_busqos *qos, u32 offset, u32 value)
{
#ifndef QOS_SECURE_ACCESS
	regmap_write(qos->regmap, offset, value);
#else
	sec_writel(qos->base + offset, value);
#endif
}

static int nx_bus_qos_parse_dt(struct platform_device *pdev,
			       struct nic_busqos *qos)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct regmap *regmap;
	struct resource	*mem;
	u32 val;

	regmap = syscon_node_to_regmap(np);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	qos->base = (void *)mem->start;
	if (IS_ERR(qos->base))
		return PTR_ERR(qos->base);

	qos->regmap = regmap;
	qos->dev = dev;

	if (!of_property_read_u32(np, "ar-fc-mode", &val))
		qos->qos_control |=
			((val & QOS_CNTL_MASK_BITS) << QOS_CNTL_AR_FC);

	if (!of_property_read_u32(np, "aw-fc-mode", &val))
		qos->qos_control |=
			((val & QOS_CNTL_MASK_BITS) << QOS_CNTL_AW_FC);

	if (!of_property_read_u32(np, "ar-fc-enable", &val))
		qos->qos_control |=
			((val & QOS_CNTL_MASK_BITS) << QOS_CNTL_EN_AR_FC);

	if (!of_property_read_u32(np, "aw-fc-enable", &val))
		qos->qos_control |=
			((val & QOS_CNTL_MASK_BITS) << QOS_CNTL_EN_AW_FC);

	if (!of_property_read_u32(np, "ar-fc-target-latency", &val))
		qos->target_fc |=
			((val & QOS_TARGET_FC_MASK_BITS) << QOS_TARGET_FC_AR);

	if (!of_property_read_u32(np, "aw-fc-target-latency", &val))
		qos->target_fc |=
			((val & QOS_TARGET_FC_MASK_BITS) << QOS_TARGET_FC_AW);

	if (!of_property_read_u32(np, "ar-max-qos", &val))
		qos->qos_range |=
			((val & QOS_RANGE_BITS) << QOS_RANGE_AR_MAX);

	if (!of_property_read_u32(np, "ar-min-qos", &val))
		qos->qos_range |=
			((val & QOS_RANGE_BITS) << QOS_RANGE_AR_MIN);

	if (!of_property_read_u32(np, "aw-max-qos", &val))
		qos->qos_range |=
			((val & QOS_RANGE_BITS) << QOS_RANGE_AW_MAX);

	if (!of_property_read_u32(np, "aw-min-qos", &val))
		qos->qos_range |=
			((val & QOS_RANGE_BITS) << QOS_RANGE_AW_MIN);

	dev_info(dev, "QoS control:0x%x, target-fc:0x%x, range:0x%x\n",
		qos->qos_control, qos->target_fc, qos->qos_range);

	return 0;
}

static void nx_bus_qos_setup(struct nic_busqos *qos)
{
	qos_writel(qos, QOS_TARGET_FC_OFFS,
		(qos->target_fc & QOS_TARGET_FC_MASK_ALL));
	qos_writel(qos, QOS_RANGE_OFFS,
		(qos->qos_range & QOS_RANGE_MASK_ALL));
	qos_writel(qos, QOS_CONTROL_OFFS,
		(qos->qos_control & QOS_CNTL_MASK_ALL));
}

static int __get_reg_by_attr(u32 type, u32 *offs, int *shift, int *size)
{
	switch (type) {
	case AR_FC_MODE:
		*offs = QOS_CONTROL_OFFS;
		*shift = 20;
		*size = 1;
		break;
	case AW_FC_MODE:
		*offs = QOS_CONTROL_OFFS;
		*shift = 16;
		*size = 1;
		break;
	case AR_FC_ENABLE:
		*offs = QOS_CONTROL_OFFS;
		*shift = 4;
		*size = 1;
		break;
	case AW_FC_ENABLE:
		*offs = QOS_CONTROL_OFFS;
		*shift = 3;
		*size = 1;
		break;
	case AR_FC_TARGET_LATENCY:
		*offs = QOS_TARGET_FC_OFFS;
		*shift = 16;
		*size = 12;
		break;
	case AW_FC_TARGET_LATENCY:
		*offs = QOS_TARGET_FC_OFFS;
		*shift = 0;
		*size = 12;
		break;
	case AR_MAX_QOS:
		*offs = QOS_RANGE_OFFS;
		*shift = 24;
		*size = 4;
		break;
	case AR_MIN_QOS:
		*offs = QOS_RANGE_OFFS;
		*shift = 16;
		*size = 4;
		break;
	case AW_MAX_QOS:
		*offs = QOS_RANGE_OFFS;
		*shift = 8;
		*size = 4;
		break;
	case AW_MIN_QOS:
		*offs = QOS_RANGE_OFFS;
		*shift = 0;
		*size = 4;
		break;
	default:
		return -EINVAL;
	};

	return 0;
}

static int nx_bus_qos_get(void *data, u64 *val)
{
	struct nx_debug_qos *d_qos = data;
	struct nic_busqos *qos = d_qos->qos;
	u32 mask, offs;
	int shift, size;
	u32 v;

	if (__get_reg_by_attr(d_qos->type, &offs, &shift, &size) < 0)
		goto out;

	v = qos_readl(qos, offs);

	mask = (1UL << size) - 1;
	*val = (v >> shift) & mask;

	dev_dbg(qos->dev, "%s [%2d]: reg %p read 0x%llx\n",
		d_qos->name, d_qos->type, qos->base + offs, *val);
out:
	return 0;
}

static int nx_bus_qos_set(void *data, u64 val)
{
	struct nx_debug_qos *d_qos = data;
	struct nic_busqos *qos = d_qos->qos;
	u32 mask, offs;
	int shift, size;
	u32 v;

	if (__get_reg_by_attr(d_qos->type, &offs, &shift, &size) < 0)
		goto out;

	v = qos_readl(qos, offs);

	mask = (1UL << size) - 1;
	/* clear bits */
	v &= ~(mask << shift);
	/* set bit */
	v |= val << shift;

	qos_writel(qos, offs, qos->target_fc);

	dev_dbg(qos->dev, "%s [%2d]: reg %p read 0x%x\n",
		d_qos->name, d_qos->type, qos->base + offs, v);

out:
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(nx_debug_qos_fops,
		nx_bus_qos_get, nx_bus_qos_set, "%llu\n");

static int nx_qos_debugfs_init(struct nic_busqos *qos)
{
	struct dentry *d_dir, *d_sub;
	struct device_node *np = qos->dev->of_node;
	umode_t mode = S_IRUGO | S_IWUSR;
	const char *dir_name = QOS_DEBUGFS_DIR;
	int i;

	d_dir = debugfs_lookup(dir_name, NULL);
	if (!d_dir) {
		d_dir = debugfs_create_dir(dir_name, NULL);
		if (IS_ERR_OR_NULL(d_dir))
			return PTR_ERR(d_dir);
	}

	d_sub = debugfs_create_dir(np->name, d_dir);
	if (IS_ERR_OR_NULL(d_sub))
		return PTR_ERR(d_sub);

	dev_dbg(qos->dev,
		"debugfs: %s/%s :%p\n", dir_name, np->name, qos->base);

	for (i = 0; i < ARRAY_SIZE(nx_debug); i++) {
		struct nx_debug_qos *d_qos =
			devm_kzalloc(qos->dev, sizeof(*d_qos), GFP_KERNEL);

		if (!d_qos)
			break;

		d_qos->name = nx_debug[i].name;
		d_qos->type = nx_debug[i].type;
		d_qos->qos = qos;

		debugfs_create_file(d_qos->name, mode,
					d_sub, d_qos, &nx_debug_qos_fops);
	}

	return 0;
}

/* platform driver interface */
static int nx_bus_qos_probe(struct platform_device *pdev)
{
	struct nic_busqos *qos;
	struct device *dev = &pdev->dev;
	int ret;

	qos = devm_kzalloc(dev, sizeof(*qos), GFP_KERNEL);
	if (!qos)
		return -ENOMEM;

	ret = nx_bus_qos_parse_dt(pdev, qos);
	if (ret)
		return ret;

	nx_bus_qos_setup(qos);

	nx_qos_debugfs_init(qos);

	platform_set_drvdata(pdev, qos);

	return 0;
}

static int nx_bus_qos_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int nx_bus_qos_suspend(struct device *dev)
{
	return 0;
}

static int nx_bus_qos_resume(struct device *dev)
{
	struct nic_busqos *qos = dev_get_drvdata(dev);

	if (qos)
		nx_bus_qos_setup(qos);

	return 0;
}

static const struct dev_pm_ops nx_bus_qos_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(nx_bus_qos_suspend, nx_bus_qos_resume)
};
#endif

static const struct of_device_id nx_bus_qos_match[] = {
	{ .compatible = "nexell,nxp3220-nic-busqos" },
	{ },
};

static struct platform_driver nx_bus_qos_driver = {
	.probe		= nx_bus_qos_probe,
	.remove		= nx_bus_qos_remove,
	.driver		= {
		.name	= "nx-nic-busqos",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
		.pm	= &nx_bus_qos_pm_ops,
#endif
		.of_match_table = nx_bus_qos_match,
	},
};

static int __init nx_bus_qos_init(void)
{
	return platform_driver_register(&nx_bus_qos_driver);
}
arch_initcall(nx_bus_qos_init);

MODULE_AUTHOR("Bon-gyu, KOO <freestyle@nexell.co.kr>");
MODULE_DESCRIPTION("BUS QoS driver for the Nexell");
MODULE_LICENSE("GPL v2");
