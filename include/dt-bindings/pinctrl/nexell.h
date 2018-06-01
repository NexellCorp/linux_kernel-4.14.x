// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/*
 * Nexell's pinctrl bindings
 *
 * (C) Copyright 2018 Nexell
 * Bongyu, KOO <freestyle@nexell.co.kr>
 */

#ifndef __DT_BINDINGS_PINCTRL_NEXELL_H__
#define __DT_BINDINGS_PINCTRL_NEXELL_H__

/* Pin function */
#define NX_PIN_FUNC0			0
#define NX_PIN_FUNC1			1
#define NX_PIN_FUNC2			2
#define NX_PIN_FUNC3			3

/* Pin pull Up/Down */
#define NX_PIN_PULL_DOWN		0
#define NX_PIN_PULL_UP			1
#define NX_PIN_PULL_NONE		2

/* Pin drive strength */
#define NX_PIN_STR0			0
#define NX_PIN_STR1			1
#define NX_PIN_STR2			2
#define NX_PIN_STR3			3

/* Pin direction */
#define NX_GPIO_INPUT			0
#define NX_GPIO_OUTPUT			1

/* Pin value */
#define NX_GPIO_LOW			0
#define NX_GPIO_HIGH			1

#endif /* __DT_BINDINGS_PINCTRL_NEXELL_H__ */
