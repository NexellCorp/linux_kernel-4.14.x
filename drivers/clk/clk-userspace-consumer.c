/*
 * Copyright (c) 2016 Akinobu Mita <akinobu.mita@gmail.com>
 *
 * This file is subject to the terms and conditions of version 2 of
 * the GNU General Public License.  See the file COPYING in the main
 * directory of this archive for more details.
 *
 * Inspired from reg-userspace-consumer
 */

#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/of.h>

struct clk_userspace_consumer {
	struct mutex lock;
	bool enabled;
	struct clk *clk;
};

static ssize_t clk_show_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct clk_userspace_consumer *consumer = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", consumer->enabled);
}

static ssize_t clk_store_enable(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct clk_userspace_consumer *consumer = dev_get_drvdata(dev);
	bool enabled;
	int ret;

	ret = strtobool(buf, &enabled);
	if (ret)
		return ret;

	mutex_lock(&consumer->lock);

	if (enabled != consumer->enabled) {
		int ret = 0;

		if (enabled) {
			ret = clk_prepare_enable(consumer->clk);
			if (ret) {
				dev_err(dev, "Failed to configure state: %d\n",
					ret);
			}
		} else {
			clk_disable_unprepare(consumer->clk);
		}

		if (!ret)
			consumer->enabled = enabled;
	}

	mutex_unlock(&consumer->lock);

	return count;
}
static DEVICE_ATTR(enable, 0644, clk_show_enable, clk_store_enable);

static ssize_t clk_show_rate(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct clk_userspace_consumer *consumer = dev_get_drvdata(dev);

	return sprintf(buf, "%ld\n", clk_get_rate(consumer->clk));
}

static ssize_t clk_store_rate(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct clk_userspace_consumer *consumer = dev_get_drvdata(dev);
	unsigned long rate;
	int err;

	err = kstrtoul(buf, 0, &rate);
	if (err)
		return err;

	err = clk_set_rate(consumer->clk, rate);
	if (err)
		return err;

	return count;
}
static DEVICE_ATTR(rate, 0644, clk_show_rate, clk_store_rate);

static struct attribute *attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_rate.attr,
	NULL,
};

static const struct attribute_group attr_group = {
	.attrs	= attributes,
};

static int clk_userspace_consumer_probe(struct platform_device *pdev)
{
	struct clk_userspace_consumer *consumer;
	int ret;

	consumer = devm_kzalloc(&pdev->dev, sizeof(*consumer), GFP_KERNEL);
	if (!consumer)
		return -ENOMEM;

	mutex_init(&consumer->lock);

	consumer->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(consumer->clk)) {
		ret = PTR_ERR(consumer->clk);
		dev_err(&pdev->dev, "Failed to get clock: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, consumer);

	ret = sysfs_create_group(&pdev->dev.kobj, &attr_group);
	if (ret)
		return ret;

	return 0;
}

static int clk_userspace_consumer_remove(struct platform_device *pdev)
{
	struct clk_userspace_consumer *consumer = platform_get_drvdata(pdev);

	sysfs_remove_group(&pdev->dev.kobj, &attr_group);

	mutex_lock(&consumer->lock);
	if (consumer->enabled)
		clk_disable_unprepare(consumer->clk);
	mutex_unlock(&consumer->lock);

	return 0;
}

#ifdef CONFIG_OF

static const struct of_device_id userspace_consumer_id[] = {
	{ .compatible = "linux,clock-userspace-consumer" },
	{ }
};
MODULE_DEVICE_TABLE(of, userspace_consumer_id);

#endif

static struct platform_driver clk_userspace_consumer_driver = {
	.probe = clk_userspace_consumer_probe,
	.remove = clk_userspace_consumer_remove,
	.driver = {
		.name = "clk-userspace-consumer",
		.of_match_table = of_match_ptr(userspace_consumer_id),
	},
};
module_platform_driver(clk_userspace_consumer_driver);

MODULE_AUTHOR("Akinobu Mita <akinobu.mita@gmail.com>");
MODULE_DESCRIPTION("Userspace consumer for common clock");
MODULE_LICENSE("GPL");
