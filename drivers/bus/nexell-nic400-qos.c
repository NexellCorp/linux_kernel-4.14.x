// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018  Nexell Co., Ltd.
 * Author: JungHyun, Kim <jhkim@nexell.co.kr>
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/mfd/syscon.h>

#define	QOS_SYSFS_DIR		"qos"

#define NIC400_CONTROL		0x10C
#define NIC400_MAX_OT		0x110
#define NIC400_MAX_COMP_OT	0x114
#define NIC400_AW_P		0x118
#define NIC400_AW_B		0x11C
#define NIC400_AW_R		0x120
#define NIC400_AR_P		0x124
#define NIC400_AR_B		0x128
#define NIC400_AR_R		0x12C
#define NIC400_TARGET_FC	0x130
#define NIC400_KI_FC		0x134
#define NIC400_RANGE		0x138

enum {
	NIC400_ID_CONTROL = 0,
	NIC400_ID_MAX_OT = 1,
	NIC400_ID_MAX_COMP_OT = 2,
	NIC400_ID_AW_P = 3,
	NIC400_ID_AW_B = 4,
	NIC400_ID_AW_R = 5,
	NIC400_ID_AR_P = 6,
	NIC400_ID_AR_B = 7,
	NIC400_ID_AR_R = 8,
	NIC400_ID_TARGET_FC = 9,
	NIC400_ID_KI_FC = 10,
	NIC400_ID_RANGE = 11,
	NIC400_ID_MAX,
};

struct nic400_data {
	u32 value[NIC400_ID_MAX];
	u32 offs[NIC400_ID_MAX];
	u32 flags;
};

struct nic400_qos {
	struct device *dev;
	struct regmap *regmap;
	phys_addr_t base;
	struct mutex lock;
	struct nic400_data f_data;
	struct nic400_data r_data;
	struct kobject *kobj;
	struct attribute_group f_attr_group;
	struct attribute_group r_attr_group;
};

struct nic400_propety {
	const char *name;
	int index;
	u32 offs, shift, size;
};

/* DT property: function control */
static struct nic400_propety nic400_f_prop[] = {
	{ "ar-fc-mode", NIC400_ID_CONTROL, NIC400_CONTROL, 20, 1 },
	{ "aw-fc-mode", NIC400_ID_CONTROL, NIC400_CONTROL, 16, 1 },
	{ "awar-ot-enb", NIC400_ID_CONTROL, NIC400_CONTROL, 7, 1 },
	{ "aw-ot-enb", NIC400_ID_CONTROL, NIC400_CONTROL, 6, 1 },
	{ "ar-ot-enb", NIC400_ID_CONTROL, NIC400_CONTROL, 5, 1 },
	{ "ar-fc-enb", NIC400_ID_CONTROL, NIC400_CONTROL, 4, 1 },
	{ "aw-fc-enb", NIC400_ID_CONTROL, NIC400_CONTROL, 3, 1 },
	{ "awar-rate-enb", NIC400_ID_CONTROL, NIC400_CONTROL, 2, 1 },
	{ "ar-rate-enb", NIC400_ID_CONTROL, NIC400_CONTROL, 1, 1 },
	{ "aw-rate-enb", NIC400_ID_CONTROL, NIC400_CONTROL, 0, 1 },

	{ "aw-p", NIC400_ID_AW_P, NIC400_AW_P, 24, 8 },
	{ "aw-b", NIC400_ID_AW_B, NIC400_AW_B, 0, 16},
	{ "aw-r", NIC400_ID_AW_R, NIC400_AW_R, 20, 12},

	{ "ar-p", NIC400_ID_AR_P, NIC400_AR_P, 24, 8 },
	{ "ar-b", NIC400_ID_AR_B, NIC400_AR_B, 0, 16},
	{ "ar-r", NIC400_ID_AR_R, NIC400_AR_R, 20, 12},

	{ "ar-fc-target-lat", NIC400_ID_TARGET_FC, NIC400_TARGET_FC, 16, 12 },
	{ "aw-fc-target-lat", NIC400_ID_TARGET_FC, NIC400_TARGET_FC, 0, 12 },

	{ "ar-max-qos", NIC400_ID_RANGE, NIC400_RANGE, 24, 4 },
	{ "ar-min-qos", NIC400_ID_RANGE, NIC400_RANGE, 16, 4 },
	{ "aw-max-qos", NIC400_ID_RANGE, NIC400_RANGE, 8, 4 },
	{ "aw-min-qos", NIC400_ID_RANGE, NIC400_RANGE, 0, 4 },
};

/* DT property: register access */
static struct nic400_propety nic400_r_prop[] = {
	{ "control", NIC400_ID_CONTROL, NIC400_CONTROL, },
	{ "max-ot", NIC400_ID_MAX_OT, NIC400_MAX_OT, },
	{ "max-comp-ot", NIC400_ID_MAX_COMP_OT, NIC400_MAX_COMP_OT, },
	{ "aw-p", NIC400_ID_AW_P, NIC400_AW_P, },
	{ "aw-b", NIC400_ID_AW_B, NIC400_AW_B, },
	{ "aw-r", NIC400_ID_AW_R, NIC400_AW_R, },
	{ "ar-p", NIC400_ID_AR_P, NIC400_AR_P, },
	{ "ar-b", NIC400_ID_AR_B, NIC400_AR_B, },
	{ "ar-r", NIC400_ID_AR_R, NIC400_AR_R, },
	{ "target-fc", NIC400_ID_TARGET_FC, NIC400_TARGET_FC, },
	{ "ki-fc", NIC400_ID_KI_FC, NIC400_KI_FC, },
	{ "range", NIC400_ID_RANGE, NIC400_RANGE, },
};

struct nic400_dev_attribute {
	struct nic400_qos *qos;
	struct device_attribute device_attr;
};

static struct kobject *nic400_kobj;

static u32 qos_readl(struct nic400_qos *qos, u32 offset)
{
	unsigned int offs = qos->base + offset;
	u32 val;

	regmap_read(qos->regmap, offs, &val);

	return val;
}

static void qos_writel(struct nic400_qos *qos, u32 offset, u32 val)
{
	unsigned int offs = qos->base + offset;

	regmap_write(qos->regmap, offs, val);
}

static int __get_property(const char *name, u32 *offs, int *shift, int *size,
			    struct nic400_propety *prop, int len)
{
	int i;

	for (i = 0; i < len; i++, prop++) {
		if (!strcmp(name, prop->name)) {
			*offs = prop->offs;
			if (shift)
				*shift = prop->shift;
			if (size)
				*size = prop->size;
			return 0;
		}
	}

	return -EINVAL;
}

static ssize_t nx_qos_f_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct nic400_dev_attribute *q_attr =
			container_of(attr, typeof(*q_attr), device_attr);
	struct nic400_qos *qos = q_attr->qos;
	struct attribute *at = &attr->attr;
	const char *name = at->name;
	u32 mask, offs;
	int shift, size;
	u32 v, val;

	if (__get_property(name, &offs, &shift, &size,
			nic400_f_prop, ARRAY_SIZE(nic400_f_prop)) < 0)
		return -EINVAL;

	v = qos_readl(qos, offs);
	mask = (1UL << size) - 1;
	val = (v >> shift) & mask;

	dev_dbg(qos->dev,
		"%s: 0x%x R:0x%x -> 0x%x [mask:0x%x, size:%d, shift:%d]\n",
		name, offs, v, val, mask, size, shift);

	return (int)scnprintf(buf, PAGE_SIZE, "0x%x\n", val);
}

static ssize_t nx_qos_f_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct nic400_dev_attribute *q_attr =
			container_of(attr, typeof(*q_attr), device_attr);
	struct nic400_qos *qos = q_attr->qos;
	struct attribute *at = &attr->attr;
	const char *name = at->name;
	u32 mask, offs;
	int shift, size;
	u32 v, val;
	int ret;

	if (__get_property(name, &offs, &shift, &size,
			nic400_f_prop, ARRAY_SIZE(nic400_f_prop)) < 0)
		return -EINVAL;

	ret = kstrtou32(buf, 0, &val);
	if (ret)
		return -EINVAL;

	v = qos_readl(qos, offs);
	mask = (1UL << size) - 1;
	v &= ~(mask << shift);
	v |= (val & mask) << shift;
	qos_writel(qos, offs, v);

	dev_dbg(qos->dev,
		"%s: 0x%x W:0x%x -> 0x%x [mask:0x%x, size:%d, shift:%d]\n",
		name, offs, val, v, mask, size, shift);

	return count;
}

struct device_attribute dev_f_attr = {
	.show = nx_qos_f_show,
	.store = nx_qos_f_store,
};

static ssize_t nx_qos_r_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct nic400_dev_attribute *q_attr =
			container_of(attr, typeof(*q_attr), device_attr);
	struct nic400_qos *qos = q_attr->qos;
	struct attribute *at = &attr->attr;
	const char *name = at->name;
	u32 offs, val;

	if (__get_property(name, &offs, NULL, NULL,
			nic400_r_prop, ARRAY_SIZE(nic400_r_prop)) < 0)
		return -EINVAL;

	val = qos_readl(qos, offs);

	dev_dbg(qos->dev, "%s: 0x%x R:0x%x\n", name, offs, val);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", val);
}

static ssize_t nx_qos_r_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct nic400_dev_attribute *q_attr =
			container_of(attr, typeof(*q_attr), device_attr);
	struct nic400_qos *qos = q_attr->qos;
	struct attribute *at = &attr->attr;
	const char *name = at->name;
	u32 offs, val;
	int ret;

	if (__get_property(name, &offs, NULL, NULL,
			nic400_r_prop, ARRAY_SIZE(nic400_r_prop)) < 0)
		return -EINVAL;

	ret = kstrtou32(buf, 0, &val);
	if (ret)
		return -EINVAL;

	qos_writel(qos, offs, val);

	dev_dbg(qos->dev, "%s: 0x%x W:0x%x\n", name, offs, val);

	return count;
}

struct device_attribute dev_r_attr = {
	.show = nx_qos_r_show,
	.store = nx_qos_r_store,
};

static void nx_qos_setup_bus(struct nic400_qos *qos)
{
	struct nic400_data *data;
	int i;

	for (data = &qos->f_data, i = 0; i < NIC400_ID_MAX; i++) {
		if (data->flags & (1 << i))
			qos_writel(qos, data->offs[i], data->value[i]);
	}

	for (data = &qos->r_data, i = 0; i < NIC400_ID_MAX; i++) {
		if (data->flags & (1 << i))
			qos_writel(qos, data->offs[i], data->value[i]);
	}
}

static int nx_qos_create_sysdir(struct nic400_qos *qos,
			    struct attribute_group *attr_group,
			    const char *name,
			    struct nic400_propety *prop, int len,
			    struct device_attribute *dev_attr)
{
	struct device_node *np = qos->dev->of_node;
	struct nic400_dev_attribute *q_attrs;
	struct attribute **attrs;
	struct device_attribute *attr;
	int i, ret;

	attrs = devm_kcalloc(qos->dev,
			len + 1, sizeof(**attrs), GFP_KERNEL);
	if (!attrs)
		return -ENOMEM;

	q_attrs = devm_kcalloc(qos->dev,
			len, sizeof(*q_attrs), GFP_KERNEL);
	if (!q_attrs)
		return -ENOMEM;

	for (i = 0; i < len; i++, q_attrs++) {
		q_attrs->qos = qos;
		attr = &q_attrs->device_attr;
		attr->attr.name = prop[i].name;
		attr->attr.mode = 0644;
		attr->show = dev_attr->show;
		attr->store = dev_attr->store;
		attrs[i] = &attr->attr;
	}

	attr_group->attrs = (struct attribute **)attrs;
	attr_group->name = name;

	if (!qos->kobj) {
		qos->kobj = kobject_create_and_add(np->name, nic400_kobj);
		if (!qos->kobj)
			return -ENOMEM;
	}

	ret = sysfs_create_group(qos->kobj, attr_group);
	if (ret)
		kobject_del(qos->kobj);

	return ret;
}

static void nx_qos_parse_register(struct device *dev, struct nic400_qos *qos)
{
	struct device_node *node = dev->of_node;
	struct nic400_propety *prop = nic400_r_prop;
	struct device_node *np;
	u32 val;
	int i, n = 0;

	np = of_get_child_by_name(node, "register");
	if (!np)
		return;

	pr_info("QoS: %s: register:\n", node->name);
	for (i = 0; i < ARRAY_SIZE(nic400_r_prop); i++, prop++) {
		if (!of_property_read_u32(np, prop->name, &val)) {
			qos->r_data.value[prop->index] = val;
			qos->r_data.offs[prop->index] = prop->offs;
			qos->r_data.flags |= (1 << prop->index);
			pr_cont(" %s:0x%08x", prop->name, val);
			n++;
			if (!(n % 4))
				pr_cont("\n");
		}
	}
	pr_cont("\n");
	of_node_put(np);
}

static void nx_qos_parse_control(struct device *dev, struct nic400_qos *qos)
{
	struct device_node *node = dev->of_node;
	struct nic400_propety *prop = nic400_f_prop;
	struct device_node *np;
	u32 val;
	int i, n = 0;

	np = of_get_child_by_name(node, "control");
	if (!np)
		return;

	pr_info("QoS: %s: control:\n", node->name);
	for (i = 0; i < ARRAY_SIZE(nic400_f_prop); i++, prop++) {
		if (!of_property_read_u32(np, prop->name, &val)) {
			qos->f_data.value[prop->index] |=
				((val & ((1UL << prop->size) - 1)) <<
				prop->shift);
			qos->f_data.offs[prop->index] = prop->offs;
			qos->f_data.flags |= (1 << prop->index);
			pr_cont(" %s:0x%x", prop->name, val);
			n++;
			if (!(n % 4))
				pr_cont("\n");
		}
	}
	pr_cont("\n");
	of_node_put(np);
}

static int nx_qos_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct nic400_qos *qos;

	qos = devm_kzalloc(dev, sizeof(*qos), GFP_KERNEL);
	if (!qos)
		return -ENOMEM;

	qos->regmap = dev_get_regmap(dev->parent, NULL);
	if (!qos->regmap)
		return -EINVAL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (IS_ERR(res))
		return PTR_ERR(res);

	qos->base = res->start;
	qos->dev = dev;
	mutex_init(&qos->lock);
	dev_set_drvdata(dev, qos);

	nx_qos_parse_control(dev, qos);
	nx_qos_parse_register(dev, qos);

	/*
	 * SYS fs:
	 * - /sys/devices/platform/qos/<node>/func
	 * - /sys/devices/platform/qos/<node>/reg
	 */
	nx_qos_create_sysdir(qos, &qos->f_attr_group, "func",
			     nic400_f_prop, ARRAY_SIZE(nic400_f_prop),
			     &dev_f_attr);

	nx_qos_create_sysdir(qos, &qos->r_attr_group, "reg",
			     nic400_r_prop, ARRAY_SIZE(nic400_r_prop),
			     &dev_r_attr);

	nx_qos_setup_bus(qos);

	platform_set_drvdata(pdev, qos);

	dev_info(&pdev->dev, "Load NIC400-QoS\n");

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
	struct nic400_qos *qos = dev_get_drvdata(dev);

	if (qos)
		nx_qos_setup_bus(qos);

	return 0;
}

static const struct dev_pm_ops nx_qos_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(nx_qos_suspend, nx_qos_resume)
};
#endif

static const struct of_device_id nx_qos_match[] = {
	{ .compatible = "nexell,nxp3220-nic400-qos" },
	{ },
};

static struct platform_driver nx_qos_driver = {
	.probe = nx_qos_probe,
	.remove = nx_qos_remove,
	.driver	= {
		.name = "nx-nic400-qos",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
		.pm = &nx_qos_pm_ops,
#endif
		.of_match_table = nx_qos_match,
	},
};

static int __init nx_qos_init(void)
{
	nic400_kobj = kobject_create_and_add(
				QOS_SYSFS_DIR, &platform_bus.kobj);
	BUG_ON(!nic400_kobj);

	return platform_driver_register(&nx_qos_driver);
}
subsys_initcall(nx_qos_init);

MODULE_AUTHOR("JungHyun, Kim <jhkim@nexell.co.kr>");
MODULE_DESCRIPTION("BUS QoS driver for the Nexell");
MODULE_LICENSE("GPL v2");
