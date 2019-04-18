// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018  Nexell Co., Ltd.
 * Author: Bon-gyu, KOO <freestyle@nexell.co.kr>
 */
#define DEBUG

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/debugfs.h>
#include <linux/pm_qos.h>
#include <linux/soc/nexell/sec_io.h>

/* for nxp3220 */
#define NR_SYSBUS_PORT		5

#define SYSBUS_QOS_CONTROL	0x10C
#define SYSBUS_TARGET_FC	0x130
#define SYSBUS_QOS_RANGE	0x138

#define SYSBUS_PORT_SHIFT	4
#define SYSBUS_ATTR_SHIFT	0
#define SYSBUS_PORT_MASK	0xf
#define SYSBUS_ATTR_MASK	0xf

struct sysbus_port {
	u32 sysbus_qos_control;
	u32 sysbus_target_fc;
	u32 sysbus_qos_range;
};

struct nx_busqos {
	void __iomem *sysbus_base;
	struct sysbus_port sysbus_port[NR_SYSBUS_PORT];
	int nr_sysbus_ports;

	struct device *dev;
};

static struct nx_busqos *busqos;

const char *bus_ports[NR_SYSBUS_PORT] =
{ "PORT_USB_HSIF", "PORT_DMA", "PORT_CPU", "PORT_DISPLAY", "PORT_ETC" };

/* sys_bus port [7:4] */
enum {
	PORT_USB_HSIF = 0,
	PORT_DMA,
	PORT_CPU,
	PORT_DISPLAY,
	PORT_ETC
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

static int nx_busqos_parse_dt(struct device *dev,
			       struct nx_busqos *nx_busqos)
{
	struct device_node *np = dev->of_node;
	struct device_node *port;
	int i, cnt = nx_busqos->nr_sysbus_ports;

	for (i = 0; i < cnt; i++) {
		u32 val;
		u32 qos_control = 0;
		u32 target_fc = 0;
		u32 qos_range = 0;

		port = of_find_node_by_name(np, bus_ports[i]);
		if (!port)
			continue;

		if (!of_property_read_u32(port, "ar_fc_mode", &val)) {
			qos_control |= (val & 0x1) << 20;
		}

		if (!of_property_read_u32(port, "aw_fc_mode", &val)) {
			qos_control |= (val & 0x1) << 16;
		}

		if (!of_property_read_u32(port, "ar_fc_enable", &val)) {
			qos_control |= (val & 0x1) << 4;
		}

		if (!of_property_read_u32(port, "aw_fc_enable", &val)) {
			qos_control |= (val & 0x1) << 3;
		}

		if (!of_property_read_u32(port, "ar_fc_target_latency", &val)) {
			target_fc |= (val & 0xfff) << 16;
		}

		if (!of_property_read_u32(port, "aw_fc_target_latency", &val)) {
			target_fc |= (val & 0xfff) << 0;
		}

		if (!of_property_read_u32(port, "ar_max_qos", &val)) {
			qos_range |= (val & 0xf) << 24;
		}

		if (!of_property_read_u32(port, "ar_min_qos", &val)) {
			qos_range |= (val & 0xf) << 16;
		}

		if (!of_property_read_u32(port, "aw_max_qos", &val)) {
			qos_range |= (val & 0xf) << 8;
		}

		if (!of_property_read_u32(port, "aw_min_qos", &val)) {
			qos_range |= (val & 0xf) << 0;
		}

		nx_busqos->sysbus_port[i].sysbus_qos_control = qos_control;
		nx_busqos->sysbus_port[i].sysbus_target_fc = target_fc;
		nx_busqos->sysbus_port[i].sysbus_qos_range = qos_range;

		dev_dbg(dev, "port [%s] qos_control : 0x%x\n", bus_ports[i],
				qos_control);
		dev_dbg(dev, "port [%s] target_fc : 0x%x\n", bus_ports[i],
				target_fc);
		dev_dbg(dev, "port [%s] qos_range : 0x%x\n", bus_ports[i],
				qos_range);
	}

	return 0;
}

static void busqos_setup(struct nx_busqos *nx_busqos)
{
	int i, cnt = nx_busqos->nr_sysbus_ports;

	for (i = 0; i < cnt; i++) {
		void *reg = nx_busqos->sysbus_base + (0x1000 * i);

		sec_writel(reg + SYSBUS_TARGET_FC,
				nx_busqos->sysbus_port[i].sysbus_target_fc);
		sec_writel(reg + SYSBUS_QOS_RANGE,
				nx_busqos->sysbus_port[i].sysbus_qos_range);
		sec_writel(reg + SYSBUS_QOS_CONTROL,
				nx_busqos->sysbus_port[i].sysbus_qos_control);
	}
}

static int __get_reg_by_attr(u32 attr, u32 *offs, int *shift, int *size)
{
	switch (attr) {
	case AR_FC_MODE:
		*offs = SYSBUS_QOS_CONTROL;
		*shift = 20;
		*size = 1;
		break;
	case AW_FC_MODE:
		*offs = SYSBUS_QOS_CONTROL;
		*shift = 16;
		*size = 1;
		break;
	case AR_FC_ENABLE:
		*offs = SYSBUS_QOS_CONTROL;
		*shift = 4;
		*size = 1;
		break;
	case AW_FC_ENABLE:
		*offs = SYSBUS_QOS_CONTROL;
		*shift = 3;
		*size = 1;
		break;
	case AR_FC_TARGET_LATENCY:
		*offs = SYSBUS_TARGET_FC;
		*shift = 16;
		*size = 12;
		break;
	case AW_FC_TARGET_LATENCY:
		*offs = SYSBUS_TARGET_FC;
		*shift = 0;
		*size = 12;
		break;
	case AR_MAX_QOS:
		*offs = SYSBUS_QOS_RANGE;
		*shift = 24;
		*size = 4;
		break;
	case AR_MIN_QOS:
		*offs = SYSBUS_QOS_RANGE;
		*shift = 16;
		*size = 4;
		break;
	case AW_MAX_QOS:
		*offs = SYSBUS_QOS_RANGE;
		*shift = 8;
		*size = 4;
		break;
	case AW_MIN_QOS:
		*offs = SYSBUS_QOS_RANGE;
		*shift = 0;
		*size = 4;
		break;
	default:
		return -EINVAL;
	};

	return 0;
}

static int nx_busqos_get(void *data, u64 *val)
{
	u32 port = (((u32)data) >> SYSBUS_PORT_SHIFT) & SYSBUS_PORT_MASK;
	u32 attr = (((u32)data) >> SYSBUS_ATTR_SHIFT) & SYSBUS_ATTR_MASK;
	u32 mask;
	u32 offs;
	int shift, size;
	unsigned long v;
	void *reg = busqos->sysbus_base + (0x1000 * port);

	if (__get_reg_by_attr(attr, &offs, &shift, &size) < 0)
		goto out;
	reg += offs;

	v = sec_readl(reg);

	mask = (1UL << size) - 1;
	*val = (v >> shift) & mask;

	pr_debug("reg %p read 0x%llx\n", reg, *val);

out:
	return 0;
}

static int nx_busqos_set(void *data, u64 val)
{
	u32 port = (((u32)data) >> SYSBUS_PORT_SHIFT) & SYSBUS_PORT_MASK;
	u32 attr = (((u32)data) >> SYSBUS_ATTR_SHIFT) & SYSBUS_ATTR_MASK;
	u32 mask;
	u32 offs;
	int shift, size;
	unsigned long v;
	void *reg = busqos->sysbus_base + (0x1000 * port);

	if (__get_reg_by_attr(attr, &offs, &shift, &size) < 0)
		goto out;
	reg += offs;

	v = sec_readl(reg);

	mask = (1UL << size) - 1;
	/* clear bits */
	v &= ~(mask << shift);
	/* set bit */
	v |= val << shift;

	sec_writel(reg, v);

	pr_debug("reg %p write 0x%lx\n", reg, v);

out:
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(nx_busqos_fops,
		nx_busqos_get, nx_busqos_set, "%llu\n");

static int nx_busqos_debugfs_init(struct nx_busqos *nx_busqos)
{
	struct dentry *d_root;
	struct dentry *d_port;
	int i, cnt = nx_busqos->nr_sysbus_ports;

	d_root = debugfs_create_dir("nx_busqos", NULL);
	if (IS_ERR_OR_NULL(d_root)) {
		d_root = NULL;
		goto err;
	}

	for (i = 0; i < cnt; i++) {
		u32 port = 0;

		d_port = debugfs_create_dir(bus_ports[i], d_root);
		if (IS_ERR_OR_NULL(d_port))
			continue;

		port = i << SYSBUS_PORT_SHIFT;

		/* sysbus_qos_control */
		debugfs_create_file("ar_fc_mode", S_IRUGO | S_IWUSR,
				d_port, (void *)(port | AR_FC_MODE),
				&nx_busqos_fops);

		debugfs_create_file("aw_fc_mode", S_IRUGO | S_IWUSR,
				d_port, (void *)(port | AW_FC_MODE),
				&nx_busqos_fops);

		debugfs_create_file("ar_fc_enable", S_IRUGO | S_IWUSR,
				d_port, (void *)(port | AR_FC_ENABLE),
				&nx_busqos_fops);

		debugfs_create_file("aw_fc_enable", S_IRUGO | S_IWUSR,
				d_port, (void *)(port | AW_FC_ENABLE),
				&nx_busqos_fops);


		/* sysbus_target_fc */
		debugfs_create_file("ar_fc_target_latency", S_IRUGO | S_IWUSR,
				d_port, (void *)(port | AR_FC_TARGET_LATENCY),
				&nx_busqos_fops);

		debugfs_create_file("aw_fc_target_latency", S_IRUGO | S_IWUSR,
				d_port, (void *)(port | AW_FC_TARGET_LATENCY),
				&nx_busqos_fops);


		/* sysbus_qos_range */
		debugfs_create_file("ar_max_qos", S_IRUGO | S_IWUSR,
				d_port, (void *)(port | AR_MAX_QOS),
				&nx_busqos_fops);

		debugfs_create_file("ar_min_qos", S_IRUGO | S_IWUSR,
				d_port, (void *)(port | AR_MIN_QOS),
				&nx_busqos_fops);

		debugfs_create_file("aw_max_qos", S_IRUGO | S_IWUSR,
				d_port, (void *)(port | AW_MAX_QOS),
				&nx_busqos_fops);

		debugfs_create_file("aw_min_qos", S_IRUGO | S_IWUSR,
				d_port, (void *)(port | AW_MIN_QOS),
				&nx_busqos_fops);
	}

err:
	return 0;
}

/* platform driver interface */
static int nx_busqos_probe(struct platform_device *pdev)
{
	struct nx_busqos *nx_busqos;
	struct resource	*mem;

	nx_busqos = devm_kzalloc(&pdev->dev, sizeof(*nx_busqos), GFP_KERNEL);
	if (!nx_busqos)
		return -ENOMEM;

	busqos = nx_busqos;
	nx_busqos->nr_sysbus_ports = ARRAY_SIZE(bus_ports);

	if (nx_busqos_parse_dt(&pdev->dev, nx_busqos))
		return -EINVAL;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	nx_busqos->sysbus_base = (void *)mem->start;
	if (IS_ERR(nx_busqos->sysbus_base))
		return PTR_ERR(nx_busqos->sysbus_base);

	busqos_setup(nx_busqos);

	nx_busqos_debugfs_init(nx_busqos);

	platform_set_drvdata(pdev, nx_busqos);
	nx_busqos->dev = &pdev->dev;

	return 0;
}

static int nx_busqos_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int nx_busqos_suspend(struct device *dev)
{
	struct nx_busqos *nx_busqos = dev_get_drvdata(dev);
	int i, cnt = nx_busqos->nr_sysbus_ports;

	if (!nx_busqos)
		return 0;

	for (i = 0; i < cnt; i++) {
		void *reg = nx_busqos->sysbus_base + (0x1000 * i);

		nx_busqos->sysbus_port[i].sysbus_target_fc =
			sec_readl(reg + SYSBUS_TARGET_FC);
		nx_busqos->sysbus_port[i].sysbus_qos_range =
			sec_readl(reg + SYSBUS_QOS_RANGE);
		nx_busqos->sysbus_port[i].sysbus_qos_control =
			sec_readl(reg + SYSBUS_QOS_CONTROL);

		sec_writel(reg + SYSBUS_TARGET_FC, 0);
		sec_writel(reg + SYSBUS_QOS_RANGE, 0);
		sec_writel(reg + SYSBUS_QOS_CONTROL, 0);
	}

	return 0;
}

static int nx_busqos_resume(struct device *dev)
{
	struct nx_busqos *nx_busqos = dev_get_drvdata(dev);

	if (nx_busqos)
		busqos_setup(nx_busqos);

	return 0;
}

static const struct dev_pm_ops nx_busqos_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(nx_busqos_suspend, nx_busqos_resume)
};
#endif

static const struct of_device_id nx_busqos_match[] = {
	{ .compatible = "nexell,nxp3220-busqos" },
	{ },
};

static struct platform_driver nx_bus_qos_driver = {
	.probe		= nx_busqos_probe,
	.remove		= nx_busqos_remove,
	.driver		= {
		.name	= "nx-busqos",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
		.pm	= &nx_busqos_pm_ops,
#endif
		.of_match_table = nx_busqos_match,
	},
};

static int __init nx_busqos_init(void)
{
	return platform_driver_register(&nx_bus_qos_driver);
}
arch_initcall(nx_busqos_init);

static void __exit nx_busqos_exit(void)
{
	platform_driver_unregister(&nx_bus_qos_driver);
}
module_exit(nx_busqos_exit);

MODULE_AUTHOR("Bon-gyu, KOO <freestyle@nexell.co.kr>");
MODULE_DESCRIPTION("BUS QoS driver for the Nexell");
MODULE_LICENSE("GPL v2");
