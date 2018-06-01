/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2018  Nexell Co., Ltd.
 * Author: Bon-gyu, KOO <freestyle@nexell.co.kr>
 */

#ifndef __PINCTRL_NXP3220_H
#define __PINCTRL_NXP3220_H

#define NR_GPIO_MODULE 5
#define GPIO_NUM_PER_BANK (32)

#define PAD_GPIO_A (0 * 32)
#define PAD_GPIO_B (1 * 32)
#define PAD_GPIO_C (2 * 32)
#define PAD_GPIO_D (3 * 32)
#define PAD_GPIO_E (4 * 32)
#define PAD_GPIO_ALV (5 * 32)

#define SOC_PIN_BANK_EINTG(pins, id)	\
	{							\
		.nr_pins	= pins,				\
		.eint_type	= EINT_TYPE_GPIO,		\
		.name		= id				\
	}

#define SOC_PIN_BANK_EINTW(pins, id)	\
	{							\
		.nr_pins	= pins,				\
		.eint_type	= EINT_TYPE_WKUP,		\
		.name		= id				\
	}

#define GPIO_OUT (0x00)
#define GPIO_OUT_ENB (0x04)
#define GPIO_INT_MODE (0x08) /* 0x08,0x0c */
#define GPIO_INT_MODEEX (0x28)
#define GPIO_INT_ENB (0x10)
#define GPIO_INT_STATUS (0x14)
#define GPIO_ALTFN (0x20) /* 0x20,0x24 */
#define GPIO_INT_DET (0x3C)
#define GPIO_IN_ENB (0x74)
#define GPIO_ALTFNEX (0x7c)

/* gpio interrupt detect type */
#define NX_GPIO_INTMODE_LOWLEVEL 0
#define NX_GPIO_INTMODE_HIGHLEVEL 1
#define NX_GPIO_INTMODE_FALLINGEDGE 2
#define NX_GPIO_INTMODE_RISINGEDGE 3
#define NX_GPIO_INTMODE_BOTHEDGE 4

#define PAD_GET_GROUP(pin) ((pin >> 0x5) & 0x07) /* Divide 32 */
#define PAD_GET_BITNO(pin) (pin & 0x1F)

/*
 * gpio descriptor
 */
#define IO_ALT_0 (0)
#define IO_ALT_1 (1)
#define IO_ALT_2 (2)
#define IO_ALT_3 (3)

/* nxp3220 GPIO function number */

#define ALT_NO_GPIO_A                                                          \
	{                                                                      \
		IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,    \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0,                                                  \
	}

#define ALT_NO_GPIO_B                                                          \
	{                                                                      \
		IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,    \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0,                                                  \
	}

#define ALT_NO_GPIO_C                                                          \
	{                                                                      \
		IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,    \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0,                                                  \
	}

#define ALT_NO_GPIO_D                                                          \
	{                                                                      \
		IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,    \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0,                                                  \
	}

#define ALT_NO_GPIO_E                                                          \
	{                                                                      \
		IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,    \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0,                                                  \
	}

#define ALT_NO_ALIVE                                                           \
	{                                                                      \
		IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,    \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0, IO_ALT_0,          \
		    IO_ALT_0,                                                  \
	}

#define nx_gpio_pull_down 0
#define nx_gpio_pull_up 1
#define nx_gpio_pull_off 2

/* GPIO Module's Register List */
struct nx_gpio_reg_set {
	/* 0x00	: Output Register */
	u32 gpio_out;
	/* 0x04	: Output Enable Register */
	u32 gpio_outenb;
	/* 0x08	: Event Detect Mode Register */
	u32 gpio_detmode[2];
	/* 0x10	: Interrupt Enable Register */
	u32 gpio_intenb;
	/* 0x14	: Event Detect Register */
	u32 gpio_det;
	/* 0x18	: PAD Status Register */
	u32 gpio_pad;
	/* 0x1C	: */
	u32 __reserved0;
	/* 0x20	: Alternate Function Select Register */
	u32 gpio_altfn[2];
	/* 0x28	: Event Detect Mode extended Register */
	u32 gpio_detmodeex;
	/* 0x2B	: */
	u32 __reserved1[4];
	/* 0x3C	: IntPend Detect Enable Register */
	u32 gpio_detenb;

	/* 0x40	: Slew Register */
	u32 gpio_slew;
	/* 0x44	: Slew set On/Off Register */
	u32 gpio_slew_disable_default;
	/* 0x48	: drive strength LSB Register */
	u32 gpio_drv1;
	/* 0x4C	: drive strength LSB set On/Off Register */
	u32 gpio_drv1_disable_default;
	/* 0x50	: drive strength MSB Register */
	u32 gpio_drv0;
	/* 0x54	: drive strength MSB set On/Off Register */
	u32 gpio_drv0_disable_default;
	/* 0x58	: Pull UP/DOWN Selection Register */
	u32 gpio_pullsel;
	/* 0x5C	: Pull UP/DOWN Selection On/Off Register */
	u32 gpio_pullsel_disable_default;
	/* 0x60	: Pull Enable/Disable Register */
	u32 gpio_pullenb;
	/* 0x64	: Pull Enable/Disable selection On/Off Register */
	u32 gpio_pullenb_disable_default;
	/* 0x68 : */
	u32 __reserved2[3];
	/* 0x74 */
	u32 gpio_inenb;
	/* 0x78 : */
	u32 gpio_inenb_disable_default;
	/* 0x7c : */
	u32 gpio_altfnex;
};

struct module_init_data {
	struct list_head node;
	void *bank_base;
	int bank_type;		/* 0: none, 1: gpio, 2: alive */
};

#endif /* __PINCTRL_NXP3220_H */
