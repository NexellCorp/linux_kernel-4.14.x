/*
 * Copyright (C) 2018  Nexell Co., Ltd.
 * Author: Ken Kim <kenkim@nexell.co.kr>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/cpu_cooling.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/pm_opp.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/of_device.h>
#include "../../drivers/base/power/opp/opp.h"
#include "cpufreq-dt.h"

#define CMU_AXI_OFF 0x264
#define CMU_ATCLK_OFF 0x268
#define CMU_CNTCLK_OFF 0x26c
#define CMU_TSCLK_OFF 0x270
#define CMU_DBGAPB_OFF 0x274
#define CMU_CPUAPB_OFF 0x278

struct private_data {
	struct opp_table *opp_table;
	struct device *cpu_dev;
	struct thermal_cooling_device *cdev;
	struct regulator *cpu_reg;
	void __iomem *base;
	const char *reg_name;
};

static struct freq_attr *cpufreq_dt_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,   /* Extra space for boost-attr if required */
	NULL,
};

static int set_cmu_divider(void __iomem *base, unsigned long freq)
{
	int div;

	if (freq <= 400000000) {
		div = 0; /* Bypass */
	} else {
		div = freq / 400000000;
		if ((freq / div ) <= 400000000)
			div -= 1;
	}
	writel(div, base + CMU_AXI_OFF);

	if (freq <= 200000000) {
		div = 0; /* Bypass */
	} else {
		div = freq / 200000000;
		if ((freq / div ) <= 200000000)
			div -= 1;
	}
	writel(div, base + CMU_ATCLK_OFF);
	writel(div, base + CMU_CNTCLK_OFF);
	writel(div, base + CMU_TSCLK_OFF);

	div = freq / 100000000;
	div -= 1;
	writel(div, base + CMU_DBGAPB_OFF);
	writel(div, base + CMU_CPUAPB_OFF);

	return 0;
}

static int _set_opp_voltage(struct device *dev, struct regulator *reg,
			    struct dev_pm_opp_supply *supply)
{
	int ret;

	/* Regulator not available for device */
	if (IS_ERR(reg)) {
		dev_dbg(dev, "%s: regulator not available: %ld\n", __func__,
			PTR_ERR(reg));
		return 0;
	}

	dev_dbg(dev, "%s: voltages (mV): %lu %lu %lu\n", __func__,
		supply->u_volt_min, supply->u_volt, supply->u_volt_max);

	ret = regulator_set_voltage_triplet(reg, supply->u_volt_min,
					    supply->u_volt, supply->u_volt_max);
	if (ret)
		dev_info(dev, "%s: failed to set voltage (%lu %lu %lu mV): %d\n",
			__func__, supply->u_volt_min, supply->u_volt,
			supply->u_volt_max, ret);

	return ret;
}

static inline int
nxp3220_set_opp_clk_only(struct device *dev, struct clk *clk,
			  void __iomem *base, unsigned long old_freq,
			  unsigned long freq)
{
	int ret;

	if (freq >= old_freq)
		set_cmu_divider(base, freq);

	ret = clk_set_rate(clk, freq);
	if (ret) {
		dev_err(dev, "%s: failed to set clock rate: %d\n", __func__,
			ret);
	}

	if (freq <= old_freq)
		set_cmu_divider(base, freq);

	return ret;
}

static int nxp3220_set_opp_regulator(struct opp_table *opp_table,
				      struct device *dev,
				      struct regulator *reg,
				      void __iomem *base,
				      unsigned long old_freq,
				      unsigned long freq,
				      struct dev_pm_opp_supply *old_supply,
				      struct dev_pm_opp_supply *new_supply)
{
	int ret;

	/* This function only supports single regulator per device */
	if (WARN_ON(opp_table->regulator_count > 1)) {
		dev_err(dev, "multiple regulators are not supported\n");
		return -EINVAL;
	}

	/* Scaling up? Scale voltage before frequency */
	if (freq >= old_freq) {
		ret = _set_opp_voltage(dev, reg, new_supply);
		if (ret)
			goto restore_voltage;
	}

	/* Change frequency */
	ret = nxp3220_set_opp_clk_only(dev, opp_table->clk, base, old_freq, freq);
	if (ret)
		goto restore_voltage;

	/* Scaling down? Scale voltage after frequency */
	if (freq < old_freq) {
		ret = _set_opp_voltage(dev, reg, new_supply);
		if (ret)
			goto restore_freq;
	}

	return 0;

restore_freq:
	if (nxp3220_set_opp_clk_only(dev, opp_table->clk, base, freq, old_freq))
		dev_err(dev, "%s: failed to restore old-freq (%lu Hz)\n",
			__func__, old_freq);
restore_voltage:
	/* This shouldn't harm even if the voltages weren't updated earlier */
	if (old_supply)
		_set_opp_voltage(dev, reg, old_supply);

	return ret;
}
/**
 * dev_pm_opp_set_rate() - Configure new OPP based on frequency
 * @dev:	 device for which we do this operation
 * @target_freq: frequency to achieve
 *
 * This configures the power-supplies and clock source to the levels specified
 * by the OPP corresponding to the target_freq.
 */
int nxp3220_pm_opp_set_rate(struct private_data *priv,
		struct device *dev, unsigned long target_freq)
{
	struct opp_table *opp_table;
	unsigned long freq, old_freq;
	struct dev_pm_opp *old_opp, *opp;
	struct clk *clk;
	int ret, size;
	//	struct regulator *reg = regulator_get(dev, priv->reg_name);
	struct regulator *reg = priv->cpu_reg;
	void __iomem *base = priv->base;

	if (unlikely(!target_freq)) {
		dev_err(dev, "%s: Invalid target frequency %lu\n", __func__,
			target_freq);
		return -EINVAL;
	}

	opp_table = dev_pm_opp_get_opp_table(dev);
	if (IS_ERR(opp_table)) {
		dev_err(dev, "%s: device opp doesn't exist\n", __func__);
		return PTR_ERR(opp_table);
	}

	clk = opp_table->clk;
	if (IS_ERR(clk)) {
		dev_err(dev, "%s: No clock available for the device\n",
			__func__);
		ret = PTR_ERR(clk);
		goto put_opp_table;
	}

	freq = clk_round_rate(clk, target_freq);
	if ((long)freq <= 0)
		freq = target_freq;

	old_freq = clk_get_rate(clk);

	/* Return early if nothing to do */
	if (old_freq == freq) {
		dev_dbg(dev, "%s: old/new frequencies (%lu Hz) are same, nothing to do\n",
			__func__, freq);
		ret = 0;
		goto put_opp_table;
	}

	old_opp = dev_pm_opp_find_freq_ceil(dev, &old_freq);
	if (IS_ERR(old_opp)) {
		dev_err(dev, "%s: failed to find current OPP for freq %lu (%ld)\n",
			__func__, old_freq, PTR_ERR(old_opp));
	}

	opp = dev_pm_opp_find_freq_ceil(dev, &freq);
	if (IS_ERR(opp)) {
		ret = PTR_ERR(opp);
		dev_err(dev, "%s: failed to find OPP for freq %lu (%d)\n",
			__func__, freq, ret);
		goto put_old_opp;
	}

	dev_dbg(dev, "%s: switching OPP: %lu Hz --> %lu Hz\n", __func__,
		old_freq, freq);

	if (!priv->cpu_reg) {
		ret = nxp3220_set_opp_clk_only(dev, clk, base, old_freq, freq);
	} else  {
		ret = nxp3220_set_opp_regulator(opp_table, dev, reg, base, old_freq, freq,
						 IS_ERR(old_opp) ? NULL : old_opp->supplies,
						 opp->supplies);
	}

	dev_pm_opp_put(opp);
put_old_opp:
	if (!IS_ERR(old_opp))
		dev_pm_opp_put(old_opp);
put_opp_table:
	dev_pm_opp_put_opp_table(opp_table);
	return ret;
}
//EXPORT_SYMBOL_GPL(dev_pm_opp_set_rate);
static int set_target(struct cpufreq_policy *policy, unsigned int index)
{
	struct private_data *priv = policy->driver_data;

	//return dev_pm_opp_set_rate(priv->cpu_dev,
	return nxp3220_pm_opp_set_rate(priv, priv->cpu_dev,
				   policy->freq_table[index].frequency * 1000);
}

/*
 * An earlier version of opp-v1 bindings used to name the regulator
 * "cpu0-supply", we still need to handle that for backwards compatibility.
 */
static const char *find_supply_name(struct device *dev)
{
	struct device_node *np;
	struct property *pp;
	int cpu = dev->id;
	const char *name = NULL;

	np = of_node_get(dev->of_node);

	/* This must be valid for sure */
	if (WARN_ON(!np))
		return NULL;

	/* Try "cpu0" for older DTs */
	if (!cpu) {
		pp = of_find_property(np, "cpu0-supply", NULL);
		if (pp) {
			name = "cpu0";
			goto node_put;
		}
	}

	pp = of_find_property(np, "cpu-supply", NULL);
	if (pp) {
		name = "cpu";
		goto node_put;
	}

	dev_dbg(dev, "no regulator for cpu%d\n", cpu);
node_put:
	of_node_put(np);
	return name;
}

static int resources_available(void)
{
	struct device *cpu_dev;
	struct regulator *cpu_reg;
	struct clk *cpu_clk;
	int ret = 0;
	const char *name;

	cpu_dev = get_cpu_device(0);
	if (!cpu_dev) {
		pr_err("failed to get cpu0 device\n");
		return -ENODEV;
	}

	cpu_clk = clk_get(cpu_dev, NULL);
	ret = PTR_ERR_OR_ZERO(cpu_clk);
	if (ret) {
		/*
		 * If cpu's clk node is present, but clock is not yet
		 * registered, we should try defering probe.
		 */
		if (ret == -EPROBE_DEFER)
			dev_dbg(cpu_dev, "clock not ready, retry\n");
		else
			dev_err(cpu_dev, "failed to get clock: %d\n", ret);

		return ret;
	}

	clk_put(cpu_clk);

	name = find_supply_name(cpu_dev);
	/* Platform doesn't require regulator */
	if (!name)
		return 0;

	cpu_reg = regulator_get_optional(cpu_dev, name);
	ret = PTR_ERR_OR_ZERO(cpu_reg);
	if (ret) {
		/*
		 * If cpu's regulator supply node is present, but regulator is
		 * not yet registered, we should try defering probe.
		 */
		if (ret == -EPROBE_DEFER)
			dev_dbg(cpu_dev, "cpu0 regulator not ready, retry\n");
		else
			dev_dbg(cpu_dev, "no regulator for cpu0: %d\n", ret);

		return ret;
	}

	regulator_put(cpu_reg);
	return 0;
}

static int cpufreq_init(struct cpufreq_policy *policy)
{
	struct cpufreq_frequency_table *freq_table;
	struct opp_table *opp_table = NULL;
	struct private_data *priv;
	struct regulator *cpu_reg = NULL;
	void __iomem *base;
	struct device *cpu_dev;
	struct clk *cpu_clk;
	unsigned int transition_latency;
	bool fallback = false;
	const char *name;
	int ret;

	cpu_dev = get_cpu_device(policy->cpu);
	if (!cpu_dev) {
		pr_err("failed to get cpu%d device\n", policy->cpu);
		return -ENODEV;
	}

	cpu_clk = clk_get(cpu_dev, NULL);
	if (IS_ERR(cpu_clk)) {
		ret = PTR_ERR(cpu_clk);
		dev_err(cpu_dev, "%s: failed to get clk: %d\n", __func__, ret);
		return ret;
	}

	/* Get OPP-sharing information from "operating-points-v2" bindings */
	ret = dev_pm_opp_of_get_sharing_cpus(cpu_dev, policy->cpus);
	if (ret) {
		if (ret != -ENOENT)
			goto out_put_clk;

		/*
		 * operating-points-v2 not supported, fallback to old method of
		 * finding shared-OPPs for backward compatibility if the
		 * platform hasn't set sharing CPUs.
		 */
		if (dev_pm_opp_get_sharing_cpus(cpu_dev, policy->cpus))
			fallback = true;
	}

	base = ioremap(0x22000000, 0x1000);
	if (IS_ERR(base)) {
		printk("GET REGS\n");
	}

	/*
	 * OPP layer will be taking care of regulators now, but it needs to know
	 * the name of the regulator first.
	 */
	name = find_supply_name(cpu_dev);
	if (name) {
		cpu_reg = regulator_get_exclusive(cpu_dev, name);
		if (IS_ERR(cpu_reg)) {
			ret = PTR_ERR(opp_table);
			dev_err(cpu_dev, "Failed to set regulator for cpu%d: %d\n",
				policy->cpu, ret);
			goto out_put_clk;
		}

	}
	/*
	 * Initialize OPP tables for all policy->cpus. They will be shared by
	 * all CPUs which have marked their CPUs shared with OPP bindings.
	 *
	 * For platforms not using operating-points-v2 bindings, we do this
	 * before updating policy->cpus. Otherwise, we will end up creating
	 * duplicate OPPs for policy->cpus.
	 *
	 * OPPs might be populated at runtime, don't check for error here
	 */
	dev_pm_opp_of_cpumask_add_table(policy->cpus);

	/*
	 * But we need OPP table to function so if it is not there let's
	 * give platform code chance to provide it for us.
	 */
	ret = dev_pm_opp_get_opp_count(cpu_dev);
	if (ret <= 0) {
		dev_dbg(cpu_dev, "OPP table is not ready, deferring probe\n");
		ret = -EPROBE_DEFER;
		goto out_free_opp;
	}

	if (fallback) {
		cpumask_setall(policy->cpus);

		/*
		 * OPP tables are initialized only for policy->cpu, do it for
		 * others as well.
		 */
		ret = dev_pm_opp_set_sharing_cpus(cpu_dev, policy->cpus);
		if (ret)
			dev_err(cpu_dev, "%s: failed to mark OPPs as shared: %d\n",
				__func__, ret);
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto out_free_opp;
	}

	priv->reg_name = name;
	priv->opp_table = opp_table;

	ret = dev_pm_opp_init_cpufreq_table(cpu_dev, &freq_table);
	if (ret) {
		dev_err(cpu_dev, "failed to init cpufreq table: %d\n", ret);
		goto out_free_priv;
	}

	priv->cpu_dev = cpu_dev;
	priv->cpu_reg = cpu_reg;
	priv->base = base;
	policy->driver_data = priv;
	policy->clk = cpu_clk;

	policy->suspend_freq = dev_pm_opp_get_suspend_opp_freq(cpu_dev) / 1000;

	ret = cpufreq_table_validate_and_show(policy, freq_table);
	if (ret) {
		dev_err(cpu_dev, "%s: invalid frequency table: %d\n", __func__,
			ret);
		goto out_free_cpufreq_table;
	}

	/* Support turbo/boost mode */
	if (policy_has_boost_freq(policy)) {
		/* This gets disabled by core on driver unregister */
		ret = cpufreq_enable_boost_support();
		if (ret)
			goto out_free_cpufreq_table;
		cpufreq_dt_attr[1] = &cpufreq_freq_attr_scaling_boost_freqs;
	}

	transition_latency = dev_pm_opp_get_max_transition_latency(cpu_dev);
	if (!transition_latency)
		transition_latency = CPUFREQ_ETERNAL;

	policy->cpuinfo.transition_latency = transition_latency;
	policy->dvfs_possible_from_any_cpu = true;

	return 0;

out_free_cpufreq_table:
	dev_pm_opp_free_cpufreq_table(cpu_dev, &freq_table);
out_free_priv:
	kfree(priv);
out_free_opp:
	dev_pm_opp_of_cpumask_remove_table(policy->cpus);
	if (name)
		dev_pm_opp_put_regulators(opp_table);
out_put_clk:
	clk_put(cpu_clk);

	return ret;
}

static int cpufreq_exit(struct cpufreq_policy *policy)
{
	struct private_data *priv = policy->driver_data;

	cpufreq_cooling_unregister(priv->cdev);
	dev_pm_opp_free_cpufreq_table(priv->cpu_dev, &policy->freq_table);
	dev_pm_opp_of_cpumask_remove_table(policy->related_cpus);
	if (priv->reg_name)
		dev_pm_opp_put_regulators(priv->opp_table);

	clk_put(policy->clk);
	kfree(priv);

	return 0;
}

static void cpufreq_ready(struct cpufreq_policy *policy)
{
	struct private_data *priv = policy->driver_data;
	struct device_node *np = of_node_get(priv->cpu_dev->of_node);

	if (WARN_ON(!np))
		return;

	/*
	 * For now, just loading the cooling device;
	 * thermal DT code takes care of matching them.
	 */
	if (of_find_property(np, "#cooling-cells", NULL)) {
		u32 power_coefficient = 0;

		of_property_read_u32(np, "dynamic-power-coefficient",
				     &power_coefficient);

		priv->cdev = of_cpufreq_power_cooling_register(np,
				policy, power_coefficient, NULL);
		if (IS_ERR(priv->cdev)) {
			dev_err(priv->cpu_dev,
				"running cpufreq without cooling device: %ld\n",
				PTR_ERR(priv->cdev));

			priv->cdev = NULL;
		}
	}

	of_node_put(np);
}

static struct cpufreq_driver dt_cpufreq_driver = {
	.flags = CPUFREQ_STICKY | CPUFREQ_NEED_INITIAL_FREQ_CHECK,
	.verify = cpufreq_generic_frequency_table_verify,
	.target_index = set_target,
	.get = cpufreq_generic_get,
	.init = cpufreq_init,
	.exit = cpufreq_exit,
	.ready = cpufreq_ready,
	.name = "cpufreq-dt",
	.attr = cpufreq_dt_attr,
	.suspend = cpufreq_generic_suspend,
};

static int dt_cpufreq_probe(struct platform_device *pdev)
{
	struct cpufreq_dt_platform_data *data = dev_get_platdata(&pdev->dev);
	int ret;

	/*
	 * All per-cluster (CPUs sharing clock/voltages) initialization is done
	 * from ->init(). In probe(), we just need to make sure that clk and
	 * regulators are available. Else defer probe and retry.
	 *
	 * FIXME: Is checking this only for CPU0 sufficient ?
	 */
	ret = resources_available();
	if (ret)
		return ret;

	if (data && data->have_governor_per_policy)
		dt_cpufreq_driver.flags |= CPUFREQ_HAVE_GOVERNOR_PER_POLICY;

	ret = cpufreq_register_driver(&dt_cpufreq_driver);
	if (ret)
		dev_err(&pdev->dev, "failed register driver: %d\n", ret);

	return ret;
}


static const struct of_device_id whitelist[] __initconst = {
	{ .compatible = "nexell,nxp3220", },
};

#if 0
static bool __init cpu0_node_has_opp_v2_prop(void)
{
	struct device_node *np = of_cpu_device_node_get(0);
	bool ret = false;

	if (of_get_property(np, "operating-points-v2", NULL))
		ret = true;

	of_node_put(np);
	return ret;
}
#endif
static int __init cpufreq_dt_platdev_init(void)
{
	struct device_node *np = of_find_node_by_path("/");
	const struct of_device_id *match;
	const void *data = NULL;

	if (!np)
		return -ENODEV;

	match = of_match_node(whitelist, np);
	if (match) {
		data = match->data;
		goto create_pdev;
	}

	of_node_put(np);
	return -ENODEV;

create_pdev:
	of_node_put(np);
	return PTR_ERR_OR_ZERO(platform_device_register_data(NULL, "cpufreq-nxp3220",
			       -1, data,
			       sizeof(struct cpufreq_dt_platform_data)));
}
device_initcall(cpufreq_dt_platdev_init);
static int dt_cpufreq_remove(struct platform_device *pdev)
{
	cpufreq_unregister_driver(&dt_cpufreq_driver);
	return 0;
}

static struct platform_driver dt_cpufreq_platdrv = {
	.driver = {
		.name	= "cpufreq-nxp3220",
	},
	.probe		= dt_cpufreq_probe,
	.remove		= dt_cpufreq_remove,
};
module_platform_driver(dt_cpufreq_platdrv);

MODULE_ALIAS("platform:cpufreq-nxp3220");
MODULE_AUTHOR("Ken Kim <kenkim@nexell.co.kr>");
MODULE_DESCRIPTION("nxp3220 cpufreq driver");
MODULE_LICENSE("GPL");
