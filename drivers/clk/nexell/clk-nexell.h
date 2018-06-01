/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Nexell SoC Common clock header
 * Copyright (c) 2018 Chanho Park <chanho61.park@samsung.com>
 */

#ifndef __CLK_NXP3220_H
#define __CLK_NXP3220_H

#include <linux/clk-provider.h>

struct nexell_clk_data {
	void __iomem *reg;
	spinlock_t lock;
	struct clk_onecell_data clk_data;
};

struct nexell_fixed_factor_clock {
	unsigned int	id;
	char		*name;
	const char	*parent_name;
	unsigned long	flags;
	unsigned int	mult;
	unsigned int	div;
};

#define FFACTOR(_id, cname, pname, m, d, f)		\
	{						\
		.id		= _id,			\
		.name		= cname,		\
		.parent_name	= pname,		\
		.flags		= f,			\
		.mult		= m,			\
		.div		= d,			\
	}

struct nexell_mux_clock {
	unsigned int	id;
	const char	*dev_name;
	const char	*name;
	const char	*const *parent_names;
	u8		num_parents;
	unsigned long	flags;
	unsigned long	offset;
	u8		shift;
	u8		width;
	u8		mux_flags;
};

#define __MUX(_id, dname, cname, pnames, o, s, w, f, mf)	\
	{							\
		.id		= _id,				\
		.dev_name	= dname,			\
		.name		= cname,			\
		.parent_names	= pnames,			\
		.num_parents	= ARRAY_SIZE(pnames),		\
		.flags		= (f) | CLK_SET_RATE_NO_REPARENT, \
		.offset		= o,				\
		.shift		= s,				\
		.width		= w,				\
		.mux_flags	= mf,				\
	}

#define MUX(_id, cname, pnames, o, s, w)			\
	__MUX(_id, NULL, cname, pnames, o, s, w, 0, 0)

#define MUX_F(_id, cname, pnames, o, s, w, f, mf)		\
	__MUX(_id, NULL, cname, pnames, o, s, w, f, mf)

struct nexell_div_clock {
	unsigned int	id;
	const char	*dev_name;
	const char	*name;
	const char	*parent_name;
	unsigned long	flags;
	unsigned long	offset;
	u8		shift;
	u8		width;
	u8		div_flags;
};

#define __DIV(_id, dname, cname, pname, o, s, w, f, df)		\
	{							\
		.id		= _id,				\
		.dev_name	= dname,			\
		.name		= cname,			\
		.parent_name	= pname,			\
		.flags		= (f) | CLK_SET_RATE_PARENT,	\
		.offset		= o,				\
		.shift		= s,				\
		.width		= w,				\
		.div_flags	= df,				\
	}

#define DIV(_id, cname, pname, o, s, w)				\
	__DIV(_id, NULL, cname, pname, o, s, w, 0, 0)

#define DIV_F(_id, cname, pname, o, s, w, f, df)		\
	__DIV(_id, NULL, cname, pname, o, s, w, f, df)

struct nexell_gate_clock {
	unsigned int		id;
	const char		*dev_name;
	const char		*name;
	const char		*parent_name;
	unsigned long		flags;
	unsigned long		offset;
	u8			bit_idx;
	u8			gate_flags;
};

#define __GATE(_id, dname, cname, pname, o, b, f, gf)		\
	{							\
		.id		= _id,				\
		.dev_name	= dname,			\
		.name		= cname,			\
		.parent_name	= pname,			\
		.flags		= f,				\
		.offset		= o,				\
		.bit_idx	= b,				\
		.gate_flags	= gf,				\
	}

/* Do not propagate clock rate setting to parent */
#define GATE_NP(_id, cname, pname, o, b, f, gf)			\
	__GATE(_id, NULL, cname, pname, o, b, f, gf)

#define GATE(_id, cname, pname, o, b, f, gf)			\
	__GATE(_id, NULL, cname, pname, o, b, (f) | CLK_SET_RATE_PARENT, gf)

#define GATE_D(_id, dname, cname, pname, o, b, f, gf)		\
	__GATE(_id, dname, cname, pname, o, b, f, gf)

struct nexell_composite_clock {
	unsigned int	id;
	const char	*dev_name;
	const char	*name;
	const char	*const *parent_names;
	u8		num_parents;
	unsigned long	flags;

	/* Mux */
	bool		has_mux;
	unsigned long	mux_offset;
	u8		mux_shift;
	u8		mux_width;
	u8		mux_flags;

	/* Divider */
	bool		has_div;
	unsigned long	div_offset;
	u8		div_shift;
	u8		div_width;
	u8		div_flags;

	/* Gate */
	bool		has_gate;
	unsigned long	gate_offset;
	u8		gate_bit_idx;
	u8		gate_flags;
};

#define COMP_BASE(_id, dname, cname, pnames, f)		\
	.id		= _id,				\
	.dev_name	= dname,			\
	.name		= cname,			\
	.parent_names	= pnames,			\
	.num_parents	= ARRAY_SIZE(pnames),		\
	.flags		= (f) | CLK_IGNORE_UNUSED,	\

#define COMP_BASE1(_id, dname, cname, pname, f)		\
	.id		= _id,				\
	.dev_name	= dname,			\
	.name		= cname,			\
	.parent_names	= (const char *[]) { pname },	\
	.num_parents	= 1,				\
	.flags		= (f),				\

#define COMP_MUX(o, s, w, mf)				\
	.has_mux	= true,				\
	.mux_offset	= o,				\
	.mux_shift	= s,				\
	.mux_width	= w,				\
	.mux_flags	= (mf) | CLK_SET_RATE_NO_REPARENT, \

#define COMP_MUX_NONE					\
	.has_mux	= false,			\

#define COMP_DIV(o, s, w, df)				\
	.has_div	= true,				\
	.div_offset	= o,				\
	.div_shift	= s,				\
	.div_width	= w,				\
	.div_flags	= df,				\

#define COMP_DIV_NONE					\
	.has_div	= false,			\

#define COMP_GATE(o, idx, gf)				\
	.has_gate	= true,				\
	.gate_offset	= o,				\
	.gate_bit_idx	= idx,				\
	.gate_flags	= gf,				\

#define COMP_GATE_NONE					\
	.has_gate	= false,			\


#define PNAME(x) static const char * const x[] __initconst

static inline void nexell_clk_add_lookup(struct clk_onecell_data *clk_data,
					 struct clk *clk, unsigned int id)
{
	if (id)
		clk_data->clks[id] = clk;
}

extern void __init
nexell_clk_register_fixed_factor(struct nexell_clk_data *ctx,
				 const struct nexell_fixed_factor_clock *list,
				 unsigned int clk_num);
extern struct nexell_clk_data *__init nexell_clk_init(void __iomem *reg,
						      unsigned long clk_num);
extern void __init
nexell_clk_register_mux(struct nexell_clk_data *ctx,
			const struct nexell_mux_clock *list,
			unsigned int clk_num);
extern void __init
nexell_clk_register_div(struct nexell_clk_data *ctx,
			const struct nexell_div_clock *list,
			unsigned int clk_num);
extern void __init
nexell_clk_register_gate(struct nexell_clk_data *ctx,
			 const struct nexell_gate_clock *list,
			 unsigned int clk_num);
extern void __init
nexell_clk_register_composite(struct nexell_clk_data *ctx,
			      const struct nexell_composite_clock *list,
			      unsigned int clk_num);
extern struct nexell_clk_data *__init
nexell_clk_init(void __iomem *reg, unsigned long clk_num);

#endif
