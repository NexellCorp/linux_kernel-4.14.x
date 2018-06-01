// SPDX-License-Identifier: GPL-2.0+
/*
 * Nexell SoC Common Clock driver
 * Copyright (c) 2018 Chanho Park <chanho61.park@samsung.com>
 */

#include <linux/slab.h>
#include <linux/clkdev.h>
#include <linux/clk.h>
#include <linux/of.h>
#include "clk-nexell.h"

void __init
nexell_clk_register_fixed_factor(struct nexell_clk_data *ctx,
				 const struct nexell_fixed_factor_clock *list,
				 unsigned int clk_num)
{
	struct clk *clk;
	const struct nexell_fixed_factor_clock *f;
	int i;

	for (i = 0; i < clk_num; i++) {
		f = &list[i];
		clk = clk_register_fixed_factor(NULL, f->name, f->parent_name,
						f->flags, f->mult, f->div);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
			       f->name);
			continue;
		}

		nexell_clk_add_lookup(&ctx->clk_data, clk, f->id);
	}
}

void __init
nexell_clk_register_mux(struct nexell_clk_data *ctx,
			const struct nexell_mux_clock *list,
			unsigned int clk_num)
{
	struct clk *clk;
	const struct nexell_mux_clock *m;
	int i;

	for (i = 0; i < clk_num; i++) {
		m = &list[i];
		clk = clk_register_mux(NULL, m->name, m->parent_names,
				       m->num_parents, m->flags,
				       ctx->reg + m->offset, m->shift,
				       m->width, m->mux_flags, &ctx->lock);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
			       m->name);
			continue;
		}

		nexell_clk_add_lookup(&ctx->clk_data, clk, m->id);
	}
}

void __init nexell_clk_register_div(struct nexell_clk_data *ctx,
				    const struct nexell_div_clock *list,
				    unsigned int clk_num)
{
	struct clk *clk;
	const struct nexell_div_clock *d;
	int i;

	for (i = 0; i < clk_num; i++) {
		d = &list[i];
		clk = clk_register_divider(NULL, d->name, d->parent_name,
					   d->flags, ctx->reg + d->offset,
					   d->shift, d->width, d->div_flags,
					   &ctx->lock);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
			       d->name);
			continue;
		}

		nexell_clk_add_lookup(&ctx->clk_data, clk, d->id);
	}
}

void __init nexell_clk_register_gate(struct nexell_clk_data *ctx,
				     const struct nexell_gate_clock *list,
				     unsigned int clk_num)
{
	struct clk *clk;
	const struct nexell_gate_clock *g;
	int i;

	for (i = 0; i < clk_num; i++) {
		g = &list[i];
		clk = clk_register_gate(NULL, g->name, g->parent_name,
					g->flags, ctx->reg + g->offset,
					g->bit_idx, g->gate_flags, &ctx->lock);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
			       g->name);
			continue;
		}

		nexell_clk_add_lookup(&ctx->clk_data, clk, g->id);
	}
}

static struct clk * __init
nexell_clk_register_composite_one(struct nexell_clk_data *ctx,
				  const struct nexell_composite_clock *comp)
{
	struct clk_mux *mux = NULL;
	struct clk_divider *div = NULL;
	struct clk_gate *gate = NULL;
	const struct clk_ops *mux_ops = NULL;
	const struct clk_ops *div_ops = NULL;
	const struct clk_ops *gate_ops = NULL;

	if (comp->has_mux) {
		mux = kzalloc(sizeof(struct clk_mux), GFP_KERNEL);
		if (!mux)
			return ERR_PTR(-ENOMEM);

		mux_ops = &clk_mux_ops;
		mux->reg = ctx->reg + comp->mux_offset;
		mux->shift = comp->mux_shift;
		mux->mask = BIT(comp->mux_width) - 1;
		mux->flags = comp->mux_flags;
		mux->lock = &ctx->lock;
	}

	if (comp->has_div) {
		div = kzalloc(sizeof(struct clk_divider), GFP_KERNEL);
		if (!div)
			goto err_free_mux;

		div_ops = &clk_divider_ops;
		div->reg = ctx->reg + comp->div_offset;
		div->shift = comp->div_shift;
		div->flags = comp->div_flags;
		div->lock = &ctx->lock;
	}

	if (comp->has_gate) {
		gate = kzalloc(sizeof(struct clk_gate), GFP_KERNEL);
		if (!gate)
			goto err_free_div;

		gate_ops = &clk_gate_ops;
		gate->reg = ctx->reg + comp->gate_offset;
		gate->bit_idx = comp->gate_bit_idx;
		gate->flags = comp->gate_flags;
		gate->lock = &ctx->lock;
	}

	return clk_register_composite(NULL, comp->name, comp->parent_names,
				      comp->num_parents, &mux->hw, mux_ops,
				      &div->hw, div_ops, &gate->hw, gate_ops,
				      comp->flags);

err_free_div:
	kfree(div);

err_free_mux:
	kfree(mux);

	return ERR_PTR(-ENOMEM);
}

void __init
nexell_clk_register_composite(struct nexell_clk_data *ctx,
			      const struct nexell_composite_clock *list,
			      unsigned int clk_num)
{
	struct clk *clk;
	int i;

	for (i = 0; i < clk_num; i++) {
		clk = nexell_clk_register_composite_one(ctx, &list[i]);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
			       list[i].name);
			continue;
		}

		nexell_clk_add_lookup(&ctx->clk_data, clk, list[i].id);
	}
}

struct nexell_clk_data *__init nexell_clk_init(void __iomem *reg,
					       unsigned long clk_num)
{
	struct nexell_clk_data *clk_data;
	struct clk **clk_table;
	int i;

	clk_data = kzalloc(sizeof(struct nexell_clk_data), GFP_KERNEL);
	if (!clk_data)
		return ERR_PTR(-ENOMEM);

	clk_table = kcalloc(clk_num, sizeof(*clk_table), GFP_KERNEL);
	if (!clk_table) {
		kfree(clk_data);
		return ERR_PTR(-ENOMEM);
	}

	clk_data->clk_data.clks = clk_table;
	clk_data->clk_data.clk_num = clk_num;
	clk_data->reg = reg;

	for (i = 0; i < clk_num; i++)
		clk_table[i] = ERR_PTR(-ENOENT);

	spin_lock_init(&clk_data->lock);

	return clk_data;
}
