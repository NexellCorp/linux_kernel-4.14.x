// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018  Nexell Co., Ltd.
 * Author: Deokjin, Lee <truevirtue@nexell.co.kr>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/of_device.h>

/* ADC X,Y Plus/Minus Channel */
#define NX_ADC_CH_TS_XP		9
#define NX_ADC_CH_TS_XM		8
#define NX_ADC_CH_TS_YP		7
#define NX_ADC_CH_TS_YM		6

/* A/D Converter Registers */
#define REG_CON(x)		((x) + 0x00)
#define REG_DAT(x)		((x) + 0x04)
#define REG_INTENB(x)		((x) + 0x08)
#define REG_INTCLR(x)		((x) + 0x0C)
#define REG_PRESCALERCON(x)	((x) + 0x10)
#define REG_TOUCHSCRRENCON(x)	((x) + 0x14)
#define REG_ADCEN(x)		((x) + 0x18)

/* ADC Control Register */
#define ADC_DATA_SEL		(0 << 10)
#define TOT_ADC_CLK_CNT		(6 <<  6)
#define ADC_CH_SEL(x)		((x >= 6) ? ((x - 2) << 3) : (x << 3))
#define ADC_STANDBY		(0 <<  2)
#define ADC_CH_START		(1 <<  0)

/*  */
#define PRESCALER_ENB		(1 << 15)

/* ADC/Touch Screen Interrupt Enable */
#define TS_INTENB		(1 <<  1)
#define ADC_INTENB		(1 <<  0)

/* ADC/Touch Screen Pending Clear */
#define TS_PENCLR		(1 <<  1)
#define ADC_PENCLR		(1 <<  0)

/* A/D Converter Enable */
#define TS_ADCENB		(1 <<  0)

/* TouchScreen Pull Up / X/Y Plus, Minus Registers */
#define TS_PUON			(1 <<  4)
#define TS_XPON			(1 <<  3)
#define TS_XMON			(1 <<  2)
#define TS_YPON			(1 <<  1)
#define TS_YMON			(1 <<  0)

/* ADC Enable Register */
#define ADC_ADCEN		(1 << 0)

/* Option Macro */
#define	TS_SLEEP_TIME		10
#define CORRECTION_VALUE	200
#define TS_COORDINATE_CNT	3

#define	TS_STAT_OPEN		(1 << 0)

#define TS_PENDOWN		0
#define TS_PENUP		1

#define X_AXIS_MIN		0
#define X_AXIS_MAX		((1 << 12) - 1)
#define Y_AXIS_MAX		X_AXIS_MAX
#define Y_AXIS_MIN		X_AXIS_MIN

struct nx_ts_info {
	struct device		*dev;
	struct input_dev	*inp;

	struct workqueue_struct	*wqueue;
	struct work_struct	work;

	void __iomem		*regs;
	struct clk		*clk;
	ulong			clk_rate;

	int			irqno;
	int			pendown;
	int			status;

	int			pre_xpos;
	int			pre_ypos;
};

static inline void nexell_ts_set_puon(struct nx_ts_info *tsi, bool on)
{
	unsigned int reg_value;

	reg_value = readl(REG_TOUCHSCRRENCON(tsi->regs));

	if (on)
		reg_value &= ~TS_PUON;
	else
		reg_value |= TS_PUON;

	writel(reg_value, REG_TOUCHSCRRENCON(tsi->regs));
}

static inline void nexell_ts_set_xp(struct nx_ts_info *tsi, bool on)
{
	unsigned int reg_value;

	reg_value = readl(REG_TOUCHSCRRENCON(tsi->regs));

	if (on)
		reg_value &= ~TS_XPON;
	else
		reg_value |= TS_XPON;

	writel(reg_value, REG_TOUCHSCRRENCON(tsi->regs));
}

static inline void nexell_ts_set_xm(struct nx_ts_info *tsi, bool on)
{
	unsigned int reg_value;

	reg_value = readl(REG_TOUCHSCRRENCON(tsi->regs));

	if (on)
		reg_value |= TS_XMON;
	else
		reg_value &= ~TS_XMON;

	writel(reg_value, REG_TOUCHSCRRENCON(tsi->regs));
}

static inline void nexell_ts_set_yp(struct nx_ts_info *tsi, bool on)
{
	unsigned int reg_value;

	reg_value = readl(REG_TOUCHSCRRENCON(tsi->regs));

	if (on)
		reg_value &= ~TS_YPON;
	else
		reg_value |= TS_YPON;

	writel(reg_value, REG_TOUCHSCRRENCON(tsi->regs));
}

static inline void nexell_ts_set_ym(struct nx_ts_info *tsi, bool on)
{
	unsigned int reg_value;

	reg_value = readl(REG_TOUCHSCRRENCON(tsi->regs));

	if (on)
		reg_value |= TS_YMON;
	else
		reg_value &= ~TS_YMON;

	writel(reg_value, REG_TOUCHSCRRENCON(tsi->regs));
}

static inline void nexell_ts_set_detect_mode(struct nx_ts_info *tsi, bool on)
{
	if (on) {
		nexell_ts_set_puon(tsi, true);
		nexell_ts_set_xp(tsi, false);
		nexell_ts_set_xm(tsi, false);
		nexell_ts_set_yp(tsi, false);
		nexell_ts_set_ym(tsi, true);
	}

	if (on)
		mdelay(1);
}

static inline bool nexell_ts_get_detect(struct nx_ts_info *tsi)
{
	return ((readl(REG_TOUCHSCRRENCON(tsi->regs)) >> 5) & 0x1);
}

static void nexell_adc_ts_ch_start(struct nx_ts_info *tsi, unsigned int ch)
{
	unsigned int reg_value;

	reg_value = readl(REG_CON(tsi->regs));
	reg_value &= ~(0x7 << 3);
	reg_value |= ((ch - 2) << 3);//ADC_CH_SEL(ch);
	writel(reg_value, REG_CON(tsi->regs));

	reg_value |= ADC_CH_START;
	writel(reg_value, REG_CON(tsi->regs));
}

static unsigned int nexell_adc_ts_read_val(struct nx_ts_info *tsi,
						unsigned int ch)
{
	unsigned int reg_value, data;
	unsigned int idle, timeout = 0xFFFFF;

	if ((ch < 6) || (ch > 9))
		return 0;

	/*
	 * If an ADC interrupt occurs, the value is read and discarded
	 * after pending clear. It can be modified later.
	 */
	reg_value = (readl(REG_INTCLR(tsi->regs)) & ADC_PENCLR);
	if (reg_value) {
		writel(reg_value, REG_INTCLR(tsi->regs));
		data = readl(REG_DAT(tsi->regs));
	}

	nexell_adc_ts_ch_start(tsi, ch);

	while (timeout-- > 0) {
		reg_value = (readl(REG_INTCLR(tsi->regs)) & ADC_PENCLR);
		if (reg_value) {
			writel(reg_value, REG_INTCLR(tsi->regs));
			do {
				idle = (readl(REG_CON(tsi->regs)) & 0x1);
			} while (idle);
			data = readl(REG_DAT(tsi->regs));
			return data;
		}
	}

	if (timeout == 0)
		return -ETIMEDOUT;

	return 0;
}

static inline void nexell_adc_ts_con(struct nx_ts_info *tsi)
{
	unsigned int reg_value;

	reg_value = (ADC_DATA_SEL | TOT_ADC_CLK_CNT |
			(0 << 3) | ADC_STANDBY | ADC_ADCEN);
	writel(reg_value, REG_CON(tsi->regs));

	reg_value = (PRESCALER_ENB | ((256 - 1) << 0));
	writel(reg_value, REG_PRESCALERCON(tsi->regs));

	reg_value = nexell_adc_ts_read_val(tsi, 0);

	writel((TS_PENCLR | ADC_PENCLR), REG_INTCLR(tsi->regs));
	writel(TS_INTENB, REG_INTENB(tsi->regs));
}

static void nexell_ts_init_base(struct nx_ts_info *tsi)
{
	nexell_adc_ts_con(tsi);

	tsi->pre_xpos = 0;
	tsi->pre_ypos = 0;
}

static inline void nexell_ts_pendown(struct input_dev *input, int tx, int ty)
{
	input_report_abs(input, ABS_X, tx);
	input_report_abs(input, ABS_Y, ty);
	input_report_abs(input, ABS_PRESSURE, 1);
	input_report_key(input, BTN_TOUCH, 1);

	input_sync(input);
}

static inline void nexell_ts_penup(struct input_dev *input)
{
	input_report_key(input, BTN_TOUCH, 0);
	input_report_abs(input, ABS_PRESSURE, 0);
	input_sync(input);
}


static int nexell_ts_read_xy(struct nx_ts_info *tsi, int *tx, int *ty)
{
	unsigned int x_ch = NX_ADC_CH_TS_XP;
	unsigned int y_ch = NX_ADC_CH_TS_YP;
	unsigned int pre_x = tsi->pre_xpos;
	unsigned int pre_y = tsi->pre_ypos;
	unsigned int x[TS_COORDINATE_CNT];
	unsigned int y[TS_COORDINATE_CNT];
	int i, diff_x, diff_y;

	*tx = 0, *ty = 0;

	nexell_ts_set_puon(tsi, false);

	nexell_ts_set_xp(tsi, true);
	nexell_ts_set_xm(tsi, true);
	nexell_ts_set_yp(tsi, false);
	nexell_ts_set_ym(tsi, false);

	mdelay(1);

	for (i = 0; i < TS_COORDINATE_CNT; i++) {
		x[i] = nexell_adc_ts_read_val(tsi, y_ch);
		*tx  += x[i];
		if (x[i] < 0) {
			pr_err("fail, read adc:%d value ...\n", y_ch);
			return -1;
		}
	}
	*tx /= TS_COORDINATE_CNT;

	nexell_ts_set_xp(tsi, false);
	nexell_ts_set_xm(tsi, false);
	nexell_ts_set_yp(tsi, true);
	nexell_ts_set_ym(tsi, true);

	mdelay(1);

	for (i = 0; i < TS_COORDINATE_CNT; i++) {
		y[i] = nexell_adc_ts_read_val(tsi, x_ch);
		*ty  += y[i];
		if (y[i] < 0) {
			pr_err("fail, read adc:%d value ...\n", x_ch);
			return -1;
		}
	}
	*ty /= TS_COORDINATE_CNT;

	if ((pre_x != 0) && (pre_y != 0)) {
		diff_x = ((pre_x > *tx) ? (pre_x - *tx) : (*tx - pre_x));
		diff_y = ((pre_y > *ty) ? (pre_y - *ty) : (*ty - pre_y));

		if (!((diff_x >= CORRECTION_VALUE)
			|| (diff_y >= CORRECTION_VALUE))) {
			tsi->pre_xpos = *tx;
			tsi->pre_ypos = *ty;
		} else {
			*tx = pre_x;
			*ty = pre_y;
		}
	} else {
		tsi->pre_xpos = *tx;
		tsi->pre_ypos = *ty;
	}

	if ((*tx < 0) || (*ty < 0))
		return -1;

	return 0;
}

static void ts_do_thread(struct work_struct *data)
{
	struct nx_ts_info *tsi = container_of(data, struct nx_ts_info, work);
	int  x = 0,  y = 0;

	while (1) {
		nexell_ts_set_detect_mode(tsi, true);

		if (nexell_ts_get_detect(tsi)) {
			tsi->pendown = TS_PENUP;
			break;
		}

		if (nexell_ts_read_xy(tsi, &x, &y) < 0)
			continue;

		nexell_ts_pendown(tsi->inp, x, y);

		msleep(TS_SLEEP_TIME);
	}

	tsi->pre_xpos = 0;
	tsi->pre_ypos = 0;

	if (tsi->pendown == TS_PENUP)
		nexell_ts_penup(tsi->inp);

	nexell_ts_set_detect_mode(tsi, true);

	writel(TS_INTENB, REG_INTENB(tsi->regs));
}

static irqreturn_t ts_do_handler(int irqno, void *dev_id)
{
	struct nx_ts_info *tsi = (struct nx_ts_info *)dev_id;

	/* clear the interrupt pending */
	writel(TS_PENCLR, REG_INTCLR(tsi->regs));

	tsi->pendown = TS_PENDOWN;

	writel(0, REG_INTENB(tsi->regs));

	schedule_work(&tsi->work);

	return IRQ_HANDLED;
}

static int nx_ts_open(struct input_dev *dev)
{
	struct nx_ts_info *tsi = input_get_drvdata(dev);

	tsi->pendown = TS_PENDOWN;
	tsi->status  = TS_STAT_OPEN;

	nexell_ts_set_detect_mode(tsi, true);

	return 0;
}

static void nx_ts_close(struct input_dev *dev)
{
	struct nx_ts_info *tsi = input_get_drvdata(dev);

	tsi->status &= ~TS_STAT_OPEN;

	nexell_ts_set_detect_mode(tsi, false);
}

static int nx_ts_probe(struct platform_device *pdev)
{
	struct nx_ts_info *tsi;
	struct input_dev *inp;
	struct resource	 *res;
	int ret = 0;

	tsi = devm_kzalloc(&pdev->dev, sizeof(struct nx_ts_info), GFP_KERNEL);
	if (!tsi)
		return -ENOMEM;

	inp = devm_input_allocate_device(&pdev->dev);
	if (!inp) {
		dev_err(&pdev->dev,
			"fail, %s allocate input device\n", pdev->name);
		return -ENXIO;
	}

	/* get the io-register address to devicetree  */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no I/O memory defined\n");
		return -ENXIO;
	}

	tsi->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(tsi->regs)) {
		ret = PTR_ERR(tsi->regs);
		dev_err(&pdev->dev, "failed to remap tsi memory: %d\n", ret);
		return ret;
	}

	tsi->clk = devm_clk_get(&pdev->dev, "adc-ts");
	if (IS_ERR(tsi->clk)) {
		dev_err(&pdev->dev, "failed getting clock for ADC(TS)\n");
		return PTR_ERR(tsi->clk);
	}
	tsi->clk_rate = clk_get_rate(tsi->clk);
	clk_prepare_enable(tsi->clk);

	tsi->irqno = platform_get_irq(pdev, 0);
	if (tsi->irqno < 0) {
		dev_err(&pdev->dev, "no tsi irq resource ? (irq: %d)\n",
			tsi->irqno);
		goto err_dev;
	}

	ret = devm_request_irq(&pdev->dev, tsi->irqno, ts_do_handler, 0,
				dev_name(&pdev->dev), tsi);
	if (ret) {
		dev_err(&pdev->dev,
			"failed requesting tsi irq %d: %d\n",
			tsi->irqno, ret);
		goto err_dev;
	}

	platform_set_drvdata(pdev, tsi);

	INIT_WORK(&tsi->work, ts_do_thread);

	tsi->dev = &pdev->dev;
	tsi->inp = inp;

	nexell_ts_init_base(tsi);

	inp->name	= "Nexell Touchscreen";
	inp->phys	= "nexell/event0";
	inp->open	= nx_ts_open;
	inp->close	= nx_ts_close;
	inp->dev.parent = &pdev->dev;

	inp->id.bustype = BUS_HOST;
	inp->id.vendor  = 0x0001;
	inp->id.product = 0x0001;
	inp->id.version = 0x0100;

	inp->absbit[0] = BIT(ABS_X) | BIT(ABS_Y);
	inp->evbit[0]  = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	inp->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	input_set_abs_params(inp, ABS_X, X_AXIS_MIN, X_AXIS_MAX, 0, 0);
	input_set_abs_params(inp, ABS_Y, Y_AXIS_MIN, Y_AXIS_MAX, 0, 0);
	input_set_abs_params(inp, ABS_PRESSURE, 0, 1, 0, 0);
	input_set_abs_params(inp, ABS_TOOL_WIDTH, 0, 1, 0, 0);

	input_set_drvdata(inp, tsi);
	ret = input_register_device(inp);
	if (ret) {
		dev_err(&pdev->dev, "fail, %s register for input device ...\n",
			pdev->name);
		goto err_dev;
	}

	return ret;
err_dev:
	input_free_device(inp);

	return ret;
}

static int nx_ts_remove(struct platform_device *pdev)
{
	struct nx_ts_info *tsi = platform_get_drvdata(pdev);

	nexell_ts_set_detect_mode(tsi, false);

	clk_disable_unprepare(tsi->clk);
	input_unregister_device(tsi->inp);

	return 0;
}

#ifdef CONFIG_PM
static int nx_ts_suspend(struct device *dev)
{
	return 0;
}

static int nx_ts_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops nx_ts_pmops = {
	.suspend	= nx_ts_suspend,
	.resume		= nx_ts_resume,
};
#endif

static const struct of_device_id nx_ts_of_match[] = {
	{ .compatible = "nexell,adc-tsc" },
	{ }
};
MODULE_DEVICE_TABLE(of, nx_ts_of_match);

static struct platform_driver nexell_adc_driver = {
	.probe		= nx_ts_probe,
	.remove		= nx_ts_remove,
	.driver		= {
		.name	= "nexell-ts",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(nx_ts_of_match),
#ifdef CONFIG_PM
		.pm	= &nx_ts_pmops,
#endif
	},
};
module_platform_driver(nexell_adc_driver);

MODULE_AUTHOR("deoks <truevirtue@nexell.co.kr>");
MODULE_DESCRIPTION("Touchscreen (with ADC) driver for the Nexell");
MODULE_LICENSE("GPL v2");
