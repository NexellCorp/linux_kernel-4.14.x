// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018  Nexell Co., Ltd.
 * Author: Bon-gyu, KOO <freestyle@nexell.co.kr>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/delay.h>

#define CHIPNAME_LEN		48

#define EFUSE_HPM_IDS0		0x530
#define EFUSE_HPM_IDS1		0x534
#define EFUSE_HPM_IDS2		0x538
#define EFUSE_HPM_IDS3		0x53C

#define SYS_REG_HPM0		0x54
#define SYS_REG_HPM1		0x58

#define HPM_ENB			0x1
#define HPM_SYNC_APM		(0x1 << 2)
#define HPM_CONFIG		(0x1f << 4)
#define HPM_RO_SEL		(0xf << 16)
#define HPM_PREDELAY		(0xf << 12)
#define HPM_SYNC_TO_APM		(1 << 12)

struct nx_ecid_reg {
	u8 chipname[CHIPNAME_LEN];	/* 0x00 */
	u32 __reserved_0x30;
	u32 guid0;			/* 0x34 */
	u16 guid1;			/* 0x38 */
	u16 guid2;			/* 0x3a */
	u8 guid3[8];			/* 0x3c */
	u32 ec0;			/* 0x44 */
	u32 __reserved_0x48;
	u32 ec2;			/* 0x4c */
	u32 __reserved_0x50[(0x100 - 0x50) / 4];
	u32 ecid[4];			/* 0x100 */
};

struct nx_guid {
	u32 id0;
	u16 id1, id2;
	u8 id3[8];
};

struct nx_hpm {
	int cpu_ro_sel;
	int cpu_cfg;
	int core_ro_sel;
	int core_cfg;
};

struct nx_ecid {
	struct device *dev;
	struct nx_ecid_reg *reg;
	struct regmap *sysreg;
	struct regmap *sefuse;
	struct regmap *shpm;
	struct clk *clk_cpu;
	struct clk *clk_core;
	struct nx_hpm hpm;
};

struct nx_ecid_attr {
	struct nx_ecid *ecid;
	struct device_attribute dev_attr;
};

static unsigned int convert_msblsb(uint32_t data, int bits)
{
	uint32_t result = 0, mask = 1;
	int i = 0;

	for (i = 0; i < bits ; i++) {
		if (data & (1 << i))
			result |= mask << (bits - i - 1);
	}

	return result;
}

static const char gst36Strtable[36] = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
	'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S',
	'T', 'U', 'V', 'W', 'X', 'Y', 'Z'
};

static void lotid_num2string(uint32_t lot_id, char str[6])
{
	uint32_t value[3], mad[3];

	value[0] = lot_id / 36;
	mad[0] = lot_id % 36;

	value[1] = value[0] / 36;
	mad[1] = value[0] % 36;

	value[2] = value[1] / 36;
	mad[2] = value[1]  % 36;

	str[0] = 'S';
	str[1] = gst36Strtable[value[2]];
	str[2] = gst36Strtable[mad[2]];
	str[3] = gst36Strtable[mad[1]];
	str[4] = gst36Strtable[mad[0]];
	str[5] = '\0';
}

static inline int _key_ready(void __iomem *reg)
{
	return (int)((readl(reg) & (1ul << 16)) >> 16);
}

static int nx_ecid_key_ready(struct nx_ecid_reg *reg)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(20);

	while (!_key_ready(&reg->ec2)) {
		if (time_after(jiffies, timeout)) {
			if (_key_ready(&reg->ec2))
				break;

			pr_err("Error: key not ready\n");
			return -EINVAL;
		}
		cpu_relax();
	}
	return 0;
}

static int nx_ecid_get_chip_name(struct nx_ecid *ecid, u8 *chip_name)
{
	struct nx_ecid_reg *reg = ecid->reg;
	u8 *c = chip_name;
	int i;

	if (nx_ecid_key_ready(reg) < 0)
		return -EBUSY;

	for (i = 0; i < CHIPNAME_LEN; i++)
		c[i] = readb(&reg->chipname[i]);

	for (i = CHIPNAME_LEN - 1; i >= 0; i--) {
		if (c[i] != '-')
			break;
		c[i] = 0;
	}

	return 0;
}

static int nx_ecid_get_ecid(struct nx_ecid *ecid, u32 id[4])
{
	struct nx_ecid_reg *reg = ecid->reg;

	if (nx_ecid_key_ready(reg) < 0)
		return -EBUSY;

	id[0] = readl(&reg->ecid[0]);
	id[1] = readl(&reg->ecid[1]);
	id[2] = readl(&reg->ecid[2]);
	id[3] = readl(&reg->ecid[3]);

	return 0;
}

static int nx_ecid_get_guid(struct nx_ecid *ecid, struct nx_guid *guid)
{
	struct nx_ecid_reg *reg = ecid->reg;

	if (nx_ecid_key_ready(reg) < 0)
		return -EBUSY;

	guid->id0 = readl(&reg->guid0);
	guid->id1 = readw(&reg->guid1);
	guid->id2 = readw(&reg->guid2);

	guid->id3[0] = readb(&reg->guid3[0]);
	guid->id3[1] = readb(&reg->guid3[1]);
	guid->id3[2] = readb(&reg->guid3[2]);
	guid->id3[3] = readb(&reg->guid3[3]);
	guid->id3[4] = readb(&reg->guid3[4]);
	guid->id3[5] = readb(&reg->guid3[5]);
	guid->id3[6] = readb(&reg->guid3[6]);
	guid->id3[7] = readb(&reg->guid3[7]);

	return 0;
}

static int nx_ecid_get_hpm_ro(struct nx_ecid *ecid, u16 hpm[8])
{
	struct regmap *map = ecid->sefuse;
	u32 buf[4] = { 0, };

	if (nx_ecid_key_ready(ecid->reg) < 0)
		return -EBUSY;

	regmap_read(map, EFUSE_HPM_IDS0, &buf[0]);
	regmap_read(map, EFUSE_HPM_IDS1, &buf[1]);
	regmap_read(map, EFUSE_HPM_IDS2, &buf[2]);
	regmap_read(map, EFUSE_HPM_IDS2, &buf[3]);

	hpm[0] = (buf[0] >> 0) & 0x3ff;
	hpm[1] = (buf[0] >> 10) & 0x3ff;
	hpm[2] = (buf[0] >> 20) & 0x3ff;
	hpm[3] = ((buf[0] >> 30) & 0x3) | ((buf[1] & 0xff) << 2);
	hpm[4] = (buf[1] >> 8) & 0x3ff;
	hpm[5] = (buf[1] >> 18) & 0x3ff;
	hpm[6] = ((buf[1] >> 28) & 0xf) | ((buf[2] & 0x3f) << 4);
	hpm[7] = (buf[2] >> 6) & 0x3ff;

	return 0;
}

static int nx_ecid_get_cpu_hpm(struct nx_ecid *ecid)
{
	struct regmap *map = ecid->shpm;
	int sel = ecid->hpm.cpu_ro_sel;
	int cfg = ecid->hpm.cpu_cfg;
	int val, ret;

	if (!IS_ERR(ecid->clk_cpu))
		clk_prepare_enable(ecid->clk_cpu);

	regmap_write(map, SYS_REG_HPM0, 0);
	val = (sel << 16) | (cfg << 4) | 0x05;
	regmap_write(map, SYS_REG_HPM0, val);

	do {
		ret = regmap_read(map, SYS_REG_HPM1, &val);
		if (ret)
			break;
	} while (!(val & HPM_SYNC_TO_APM));

	regmap_read(map, SYS_REG_HPM1, &val);

	if (!IS_ERR(ecid->clk_cpu))
		clk_disable_unprepare(ecid->clk_cpu);

	return val & 0x3ff;
}

static int nx_ecid_get_core_hpm(struct nx_ecid *ecid)
{
	struct regmap *map = ecid->sysreg;
	int sel = ecid->hpm.core_ro_sel;
	int cfg = ecid->hpm.core_cfg;
	int val;

	if (!IS_ERR(ecid->clk_core))
		clk_prepare_enable(ecid->clk_core);

	regmap_update_bits(map, 0x404, HPM_ENB, 0);
	regmap_update_bits(map, 0x404, HPM_CONFIG, cfg << 4);
	regmap_update_bits(map, 0x404, HPM_RO_SEL, sel << 16);
	regmap_update_bits(map, 0x404, HPM_PREDELAY, 0);
	regmap_update_bits(map, 0x404, HPM_SYNC_APM, 0);

	regmap_update_bits(map, 0x404, HPM_SYNC_APM, 1<<2);
	regmap_read(map, 0x404, &val);
	regmap_update_bits(map, 0x404, HPM_ENB, 1);

	regmap_read(map, 0x404, &val);

	do {
		regmap_read(map, 0x408, &val);
	} while (!(val & HPM_SYNC_TO_APM));

	if (!IS_ERR(ecid->clk_core))
		clk_disable_unprepare(ecid->clk_core);

	return val & 0x3ff;
}

/*
 * Notify ecid GUID: /sys/devices/platform/cpu/ guid, uuid, name
 */
static ssize_t sys_ecid_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct nx_ecid_attr *p = container_of(attr, typeof(*p), dev_attr);
	struct nx_ecid *ecid = p->ecid;
	struct attribute *at = &attr->attr;
	char *s = buf;
	u32 id[4] = {0, };
	u8 chipname[CHIPNAME_LEN + 1] = {0, };
	int string = 0;
	int ret;

	if (!strncmp(at->name, "uuid", 4)) {
		ret = nx_ecid_get_ecid(ecid, id);
	} else if (!strncmp(at->name, "guid", 4)) {
		ret = nx_ecid_get_guid(ecid, (struct nx_guid *)id);
	} else if (!strncmp(at->name, "name", 4)) {
		ret = nx_ecid_get_chip_name(ecid, chipname);
		string = 1;
	} else {
		return -EINVAL;
	}

	if (ret < 0)
		return ret;

	if (string) {
		if (isprint(chipname[0])) {
			s += snprintf(s, sizeof(chipname), "%s\n", chipname);
		} else {
			#define _W	(12)	/* width */
			int i;

			for (i = 0; i < CHIPNAME_LEN; i++) {
				s += snprintf(s, 2, "%02x", chipname[i]);
				if ((i + 1) % _W == 0)
					s += snprintf(s, 1, " ");
			}
			s += snprintf(s, 1, "\n");
		}
	} else {
		s += snprintf(s, 36, "%08x:%08x:%08x:%08x\n",
			      id[0], id[1], id[2], id[3]);
	}

	if (s != buf)
		*(s - 1) = '\n';

	return (s - buf);
}

static ssize_t sys_asv_ids_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct nx_ecid_attr *p = container_of(attr, typeof(*p), dev_attr);
	struct nx_ecid *ecid = p->ecid;
	char *s = buf;
	u32 id[4] = {0, };
	u32 cpu_ids, core_ids;
	int cpu_frac, cpu_inte;
	int core_frac, core_inte;
	int len, ret;

	ret = nx_ecid_get_ecid(ecid, id);
	if (ret < 0)
		return ret;

	cpu_ids = (id[1] >> 16) & 0xff;
	cpu_frac = ((id[1] >> 16) & 0x3) * 25;
	cpu_inte = ((id[1] >> 18) & 0x3f);

	core_ids = (id[1] >> 24) & 0xff;
	core_frac = ((id[1] >> 24) & 0x3) * 25;
	core_inte = ((id[1] >> 26) & 0x3f);

	s += snprintf(s, 7, "%02x:%02x ", cpu_ids, core_ids);
	len = snprintf(NULL, 0, "cpu: %2d.%02d mA, ", cpu_inte, cpu_frac);
	s += snprintf(s, len + 1, "cpu: %2d.%02d mA, ", cpu_inte, cpu_frac);
	len = snprintf(NULL, 0, "core: %2d.%02d mA\n", core_inte, core_frac);
	s += snprintf(s, len + 1, "core: %2d.%02d mA\n", core_inte, core_frac);

	if (s != buf)
		*(s-1) = '\n';

	return (s - buf);
}

static ssize_t sys_asv_ro_show(struct device *pdev,
			       struct device_attribute *attr, char *buf)
{
	struct nx_ecid_attr *p = container_of(attr, typeof(*p), dev_attr);
	struct nx_ecid *ecid = p->ecid;
	char *s = buf;
	u16 hpm[8] = {0, };
	int len, ret;

	ret = nx_ecid_get_hpm_ro(ecid, hpm);
	if (ret < 0)
		return ret;

	len = snprintf(NULL, 0, "%03x:%03x:%03x:%03x:%03x:%03x:%03x:%03x\n",
			hpm[0], hpm[1], hpm[2], hpm[3],
			hpm[4], hpm[5], hpm[6], hpm[7]);
	s += snprintf(s, len + 1, "%03x:%03x:%03x:%03x:%03x:%03x:%03x:%03x\n",
			hpm[0], hpm[1], hpm[2], hpm[3],
			hpm[4], hpm[5], hpm[6], hpm[7]);

	if (s != buf)
		*(s-1) = '\n';

	return (s - buf);
}

static ssize_t sys_cpu_hpm_show(struct device *pdev,
				struct device_attribute *attr, char *buf)
{
	struct nx_ecid_attr *p = container_of(attr, typeof(*p), dev_attr);
	struct nx_ecid *ecid = p->ecid;
	char *s = buf;
	int val;

	val = nx_ecid_get_cpu_hpm(ecid);
	if (val < 0)
		return val;

	s += snprintf(s, 6, "%03x\n", val);

	return (s - buf);
}

static ssize_t sys_core_hpm_show(struct device *pdev,
				 struct device_attribute *attr, char *buf)
{
	struct nx_ecid_attr *p = container_of(attr, typeof(*p), dev_attr);
	struct nx_ecid *ecid = p->ecid;
	char *s = buf;
	int val;

	val = nx_ecid_get_core_hpm(ecid);
	if (val < 0)
		return val;

	s += snprintf(s, 6, "%03x\n", val);

	return (s - buf);
}

static ssize_t sys_cpu_ro_sel_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct nx_ecid_attr *p = container_of(attr, typeof(*p), dev_attr);
	struct nx_ecid *ecid = p->ecid;
	char *s = buf;

	s += snprintf(s, 6, "%03d\n", ecid->hpm.cpu_ro_sel);

	return (s - buf);
}

static ssize_t sys_cpu_ro_sel_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct nx_ecid_attr *p = container_of(attr, typeof(*p), dev_attr);
	struct nx_ecid *ecid = p->ecid;
	int ret;

	ret = kstrtouint(buf, 0, &ecid->hpm.cpu_ro_sel);

	return count;
}

static ssize_t sys_core_ro_sel_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct nx_ecid_attr *p = container_of(attr, typeof(*p), dev_attr);
	struct nx_ecid *ecid = p->ecid;
	char *s = buf;

	s += snprintf(s, 6, "%03d\n", ecid->hpm.core_ro_sel);

	return (s - buf);

}

static ssize_t sys_core_ro_sel_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct nx_ecid_attr *p = container_of(attr, typeof(*p), dev_attr);
	struct nx_ecid *ecid = p->ecid;
	int ret;

	ret = kstrtouint(buf, 0, &ecid->hpm.core_ro_sel);

	return count;
}

static ssize_t sys_cpu_config_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct nx_ecid_attr *p = container_of(attr, typeof(*p), dev_attr);
	struct nx_ecid *ecid = p->ecid;
	char *s = buf;

	s += snprintf(s, 6, "%03d\n", ecid->hpm.cpu_cfg);

	return (s - buf);

}

static ssize_t sys_cpu_config_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct nx_ecid_attr *p = container_of(attr, typeof(*p), dev_attr);
	struct nx_ecid *ecid = p->ecid;
	int ret;

	ret = kstrtouint(buf, 0, &ecid->hpm.cpu_cfg);

	return count;
}

static ssize_t sys_core_config_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct nx_ecid_attr *p = container_of(attr, typeof(*p), dev_attr);
	struct nx_ecid *ecid = p->ecid;
	char *s = buf;

	s += snprintf(s, 6, "%03d\n", ecid->hpm.core_cfg);

	return (s - buf);

}

static ssize_t sys_core_config_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct nx_ecid_attr *p = container_of(attr, typeof(*p), dev_attr);
	struct nx_ecid *ecid = p->ecid;
	int ret;

	ret = kstrtouint(buf, 0, &ecid->hpm.core_cfg);

	return count;
}

#define CPUID_ATTR(_name, _mode, _show, _store)			\
	struct nx_ecid_attr dev_attr_##_name = {		\
		.dev_attr = __ATTR(_name, _mode, _show, _store), \
	}

#define CPUID_ATTR_PTR(_name) (&dev_attr_##_name.dev_attr.attr)

static CPUID_ATTR(guid, 0444, sys_ecid_show, NULL);
static CPUID_ATTR(uuid, 0444, sys_ecid_show, NULL);
static CPUID_ATTR(name, 0444, sys_ecid_show, NULL);
static CPUID_ATTR(ids, 0444, sys_asv_ids_show, NULL);
static CPUID_ATTR(ro, 0444, sys_asv_ro_show, NULL);
static CPUID_ATTR(cpu_hpm, 0444, sys_cpu_hpm_show, NULL);
static CPUID_ATTR(core_hpm, 0444, sys_core_hpm_show, NULL);
static CPUID_ATTR(cpu_ro_sel, 0644, sys_cpu_ro_sel_show,
				    sys_cpu_ro_sel_store);
static CPUID_ATTR(cpu_config, 0644, sys_cpu_config_show,
				    sys_cpu_config_store);
static CPUID_ATTR(core_ro_sel, 0644, sys_core_ro_sel_show,
				     sys_core_ro_sel_store);
static CPUID_ATTR(core_config, 0644, sys_core_config_show,
				     sys_core_config_store);

static struct attribute *cpuid_attrs[] = {
	CPUID_ATTR_PTR(guid),
	CPUID_ATTR_PTR(uuid),
	CPUID_ATTR_PTR(name),
	CPUID_ATTR_PTR(ids),
	CPUID_ATTR_PTR(ro),
	CPUID_ATTR_PTR(cpu_hpm),
	CPUID_ATTR_PTR(core_hpm),
	CPUID_ATTR_PTR(cpu_ro_sel),
	CPUID_ATTR_PTR(cpu_config),
	CPUID_ATTR_PTR(core_ro_sel),
	CPUID_ATTR_PTR(core_config),
	NULL,
};

static struct attribute_group cpuid_attr_group = {
	.attrs = (struct attribute **)cpuid_attrs,
};

/*
 * Export functions
 */
static struct nx_ecid *_ecid_ptr;

int read_cpu_hpm(void)
{
	return nx_ecid_get_cpu_hpm(_ecid_ptr);
}

int nx_cpu_id_ecid(u32 id[4])
{
	return nx_ecid_get_ecid(_ecid_ptr, id);
}

int nx_cpu_hpm_ro(u16 hpm[8])
{
	return nx_ecid_get_hpm_ro(_ecid_ptr, hpm);
}

/*
 * Regisetr driver
 */
static struct kobject *nx_ecid_kobj;

static int nx_ecid_sysfs_create(struct device *dev, struct nx_ecid *ecid,
				    struct attribute_group *group)
{
	struct device_attribute *d_attr;
	struct nx_ecid_attr *p_attr;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(cpuid_attrs) - 1; i++) {
		d_attr = container_of(cpuid_attrs[i], typeof(*d_attr), attr);
		p_attr = container_of(d_attr, typeof(*p_attr), dev_attr);
		p_attr->ecid = ecid;
	}

	ret = sysfs_create_group(nx_ecid_kobj, group);
	if (ret)
		kobject_del(nx_ecid_kobj);

	return ret;
}

static int nx_ecid_dt_parse(struct platform_device *pdev, struct nx_ecid *ecid)
{
	struct device *dev = &pdev->dev;
	struct platform_device *plat_dev;
	struct device_node *node, *np;
	struct resource *res;

	node = dev_of_node(dev);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	ecid->reg = devm_ioremap_resource(dev, res);
	if (IS_ERR(ecid->reg))
		return PTR_ERR(ecid->reg);

	ecid->sysreg = syscon_regmap_lookup_by_phandle(node, "syscon");
	if (IS_ERR(ecid->sysreg))
		return PTR_ERR(ecid->sysreg);

	np = of_parse_phandle(node, "regmap", 0);
	if (!np)
		return -EINVAL;

	plat_dev = of_find_device_by_node(np);
	if (plat_dev)
		ecid->sefuse = dev_get_regmap(&plat_dev->dev, NULL);

	of_node_put(np);

	if (!ecid->sefuse)
		return -EINVAL;

	np = of_parse_phandle(node, "regmap", 1);
	if (!np)
		return -EINVAL;

	plat_dev = of_find_device_by_node(np);
	if (plat_dev)
		ecid->shpm = dev_get_regmap(&plat_dev->dev, NULL);

	of_node_put(np);

	if (!ecid->shpm)
		return -EINVAL;

	ecid->clk_cpu = of_clk_get_by_name(node, "cpu_hpm");
	if (IS_ERR(ecid->clk_cpu))
		return PTR_ERR(ecid->clk_cpu);

	ecid->clk_core = of_clk_get_by_name(node, "core_hpm");
	if (IS_ERR(ecid->clk_core))
		return PTR_ERR(ecid->clk_core);

	return 0;
}

static int nx_ecid_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct nx_ecid *ecid;
	u32 id[4] = { 0, };
	u32 lotid;
	char strlotid[6];
	int ret;

	ecid = devm_kzalloc(dev, sizeof(*ecid), GFP_KERNEL);
	if (!ecid)
		return -ENOMEM;

	ret = nx_ecid_dt_parse(pdev, ecid);
	if (!ecid)
		return ret;

	ret = nx_ecid_sysfs_create(dev, ecid, &cpuid_attr_group);
	if (ret)
		return ret;

	ecid->dev = dev;
	ecid->hpm.cpu_ro_sel = 0;
	ecid->hpm.cpu_cfg = 3;
	ecid->hpm.core_ro_sel = 0;
	ecid->hpm.core_cfg = 3;
	_ecid_ptr = ecid; /* global */

	if (nx_ecid_get_ecid(ecid, id) < 0)
		pr_err("FAIL: cannot get ecid !!!\n");

	lotid = convert_msblsb(id[0] & 0x1FFFFF, 21);
	lotid_num2string(lotid, strlotid);

	pr_info("ECID: %08x:%08x:%08x:%08x\n", id[0], id[1], id[2], id[3]);
	pr_info("LOT ID : %s\n", strlotid);

	return ret;
}

static const struct of_device_id nxp3220_cpuid_match[] = {
	{ .compatible = "nexell,nxp3220-ecid" },
	{ /* sentinel */ }
};

static struct platform_driver nx_ecid_driver = {
	.probe = nx_ecid_probe,
	.driver = {
		.name = "nexell-ecid",
		.of_match_table = nxp3220_cpuid_match,
	},
};

static int __init nx_ecid_init(void)
{
	nx_ecid_kobj = kobject_create_and_add("cpu", &platform_bus.kobj);

	BUG_ON(!nx_ecid_kobj);

	return platform_driver_register(&nx_ecid_driver);
}

static void __exit nx_ecid_exit(void)
{
	platform_driver_unregister(&nx_ecid_driver);
}

subsys_initcall(nx_ecid_init);
module_exit(nx_ecid_exit);

MODULE_DESCRIPTION("Nexell ECID driver");
MODULE_LICENSE("GPL v2");
