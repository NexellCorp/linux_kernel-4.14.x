// SPDX-License-Identifier: GPL-2.0+
/*
 * SM5011 Regulator driver
 *
 * Copyright (c) 2018 Chanho Park <chanho61.park@samsung.com>
 */

#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/mfd/sm5011.h>
#include <linux/mfd/sm5011-private.h>

#define SM5011_OPMODE_OFF		0x0
#define SM5011_OPMODE_ONOFF_TRANS	0x1
#define SM5011_OPMODE_ON		0x3

#define SM5011_RAMP_RATE_MASK		0x18
#define SM5011_RAMP_RATE_SHIFT		3

static int sm5011_enable(struct regulator_dev *rdev)
{
	unsigned int val;

	/* TODO: check pin status of nPWRSTM */

	val = SM5011_OPMODE_ON;

	return regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
				  rdev->desc->enable_mask, val);
}

static int sm5011_disable(struct regulator_dev *rdev)
{
	unsigned int val;

	/* TODO: check pin status of nPWRSTM */

	val = SM5011_OPMODE_OFF;

	return regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
				  rdev->desc->enable_mask, val);
}

static int sm5011_set_ramp_delay(struct regulator_dev *rdev, int ramp_delay)
{
	unsigned int ramp_reg = rdev->desc->enable_reg + 2;
	unsigned int ramp_val;

	switch (ramp_delay) {
	case 1 ... 3125:
		ramp_val = 0;
		break;
	case 3126 ... 6250:
		ramp_val = 1;
		break;
	case 6251 ... 12500:
		ramp_val = 2;
		break;
	case 12501 ... 25000:
		ramp_val = 3;
		break;
	default:
		pr_warn("ramp_delay: %d is not suported, setting 3125\n",
			ramp_delay);
		ramp_val = 0;
	}

	return regmap_update_bits(rdev->regmap, ramp_reg,
				  SM5011_RAMP_RATE_MASK,
				  ramp_val << SM5011_RAMP_RATE_SHIFT);
}

static int sm5011_of_parse_cb(struct device_node *np,
			      const struct regulator_desc *desc,
			      struct regulator_config *config)
{
	/* TODO: get custom properties */

	return 0;
}

static struct regulator_ops sm5011_buck_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.map_voltage = regulator_map_voltage_linear,
	.is_enabled = regulator_is_enabled_regmap,
	.enable = sm5011_enable,
	.disable = sm5011_disable,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.set_ramp_delay = sm5011_set_ramp_delay,
};

#define SM5011_BUCK_MIN_UV	562500
#define SM5011_BUCK_UV_STEP	12500
#define SM5011_BUCK_VSEL_MASK	0xFF
#define SM5011_BUCK_RAMP_DELAY	31250		/* uV/us */
#define SM5011_BUCK_ENABLE_MASK	0x03

#define SM5011_BUCK_DESC(num) {	\
	.name		= "BUCK"#num,	\
	.of_match	= "BUCK"#num,	\
	.regulators_node	= of_match_ptr("voltage-regulators"),	\
	.id		= SM5011_BUCK##num,	\
	.ops		= &sm5011_buck_ops,	\
	.type		= REGULATOR_VOLTAGE,	\
	.min_uV		= SM5011_BUCK_MIN_UV,	\
	.uV_step	= SM5011_BUCK_UV_STEP,	\
	.ramp_delay	= SM5011_BUCK_RAMP_DELAY,	\
	.n_voltages	= SM5011_BUCK_VSEL_MASK + 1,	\
	.vsel_reg	= SM5011_REG_BUCK##num##_CNTL2,	\
	.vsel_mask	= SM5011_BUCK_VSEL_MASK,	\
	.enable_reg	= SM5011_REG_BUCK##num##_CNTL1,	\
	.enable_mask	= SM5011_BUCK_ENABLE_MASK,	\
}

static struct regulator_ops sm5011_ldo_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.map_voltage = regulator_map_voltage_linear,
	.is_enabled = regulator_is_enabled_regmap,
	.enable = sm5011_enable,
	.disable = sm5011_disable,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
};

#define SM5011_LDO_MIN_UV	800000
#define SM5011_LDO_UV_STEP	50000
#define SM5011_LDO_VSEL_MASK	0x3F
#define SM5011_LDO_ENABLE_MASK	0x3

#define SM5011_LDO_DESC(num) {	\
	.name		= "LDO"#num,	\
	.of_match	= "LDO"#num,	\
	.regulators_node	= of_match_ptr("voltage-regulators"),	\
	.of_parse_cb	= sm5011_of_parse_cb,	\
	.id		= SM5011_LDO##num,	\
	.ops		= &sm5011_ldo_ops,	\
	.type		= REGULATOR_VOLTAGE,	\
	.min_uV		= SM5011_LDO_MIN_UV,	\
	.uV_step	= SM5011_LDO_UV_STEP,	\
	.n_voltages	= SM5011_LDO_VSEL_MASK + 1,	\
	.vsel_reg	= SM5011_REG_LDO##num##_CNTL2,	\
	.vsel_mask	= SM5011_LDO_VSEL_MASK,		\
	.enable_reg	= SM5011_REG_LDO##num##_CNTL1,	\
	.enable_mask	= SM5011_LDO_ENABLE_MASK,	\
}

static struct regulator_desc regulators[] = {
	/* Bucks */
	SM5011_BUCK_DESC(2),
	SM5011_BUCK_DESC(3),
	SM5011_BUCK_DESC(4),
	SM5011_BUCK_DESC(5),
	SM5011_BUCK_DESC(6),
	/* LDOs */
	SM5011_LDO_DESC(1),
	SM5011_LDO_DESC(2),
	SM5011_LDO_DESC(3),
	SM5011_LDO_DESC(4),
	SM5011_LDO_DESC(5),
	SM5011_LDO_DESC(6),
	SM5011_LDO_DESC(7),
	SM5011_LDO_DESC(8),
	SM5011_LDO_DESC(9),
	SM5011_LDO_DESC(10),
	SM5011_LDO_DESC(11),
	SM5011_LDO_DESC(12),
	SM5011_LDO_DESC(13),
	SM5011_LDO_DESC(14),
	SM5011_LDO_DESC(15),
	SM5011_LDO_DESC(16),
	SM5011_LDO_DESC(17),
	SM5011_LDO_DESC(18),
	SM5011_LDO_DESC(19),
	SM5011_LDO_DESC(20),
};

static int sm5011_pmic_probe(struct platform_device *pdev)
{
	struct sm5011_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct regulator_config config = {};
	int i;

	config.dev = iodev->dev;
	config.regmap = iodev->regmap;

	for (i = 0; i < SM5011_REG_MAX; i++) {
		struct regulator_dev *rdev;
		int id = regulators[i].id;

		rdev = devm_regulator_register(&pdev->dev, &regulators[i],
					       &config);
		if (IS_ERR(rdev)) {
			int ret = PTR_ERR(rdev);
			dev_err(&pdev->dev, "regulator %d init failed: %d\n",
				id, ret);
			return ret;
		}
	}

	return 0;
}

static const struct platform_device_id sm5011_pmic_id[] = {
	{"sm5011-pmic", 0},
	{ },
};

static struct platform_driver sm5011_pmic_driver = {
	.driver = {
		.name = "sm5011-pmic",
	},
	.probe = sm5011_pmic_probe,
	.id_table = sm5011_pmic_id,
};

module_platform_driver(sm5011_pmic_driver);

MODULE_DESCRIPTION("SM5011 Regulator Driver");
MODULE_AUTHOR("Chanho Park <chanho61.park@samsung.com>");
MODULE_LICENSE("GPL");
