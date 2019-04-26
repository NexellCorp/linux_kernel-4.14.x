// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018  Nexell Co., Ltd.
 * Author: Jongkeun, Choi <jkchoi@nexell.co.kr>
 */

#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>

#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>

/*#define DEBUG_TP2825*/
#ifdef DEBUG_TP2825
#define vmsg(a...)  printk(a)
#else
#define vmsg(a...)
#endif

/* #define BRIGHTNESS_TEST */
#define DEFAULT_BRIGHTNESS  0x1e
struct tp2825_state {
	struct media_pad pad;
	struct v4l2_subdev sd;
	bool first;

	struct i2c_client *i2c_client;

	struct v4l2_ctrl_handler handler;

	/* custom control */
	struct v4l2_ctrl *ctrl_mux;
	struct v4l2_ctrl *ctrl_status;

	/* standard control */
	struct v4l2_ctrl *ctrl_brightness;
	int brightness;
};

struct reg_val {
	uint8_t reg;
	uint8_t val;
};

#define END_MARKER {0xff, 0xff}

static struct reg_val _sensor_init_data[] = {
	{0x40, 0x00},
	{0x07, 0xc0},
	{0x0b, 0xc0},
	{0x39, 0x8c},
	{0x4d, 0x03},
	{0x4e, 0x17},
	/* PTZ */
	{0xc8, 0x21},
	{0x7e, 0x01},
	{0xb9, 0x01},
	/* Data Set */
	{0x4e, 0x17},
	{0x02, 0xcf},
	{0x15, 0x13},
	{0x16, 0x4e},
	{0x17, 0x80},
	{0x18, 0x13},
	{0x19, 0xf0},
	{0x1a, 0x07},
	{0x1c, 0x09},
	{0x1d, 0x38},

	{0x0c, 0x53},
	{0x0d, 0x10},
	{0x20, 0xa0},
	{0x26, 0x12},
	{0x2b, 0x70},
	{0x2d, 0x68},
	{0x2e, 0x5e},

	{0x30, 0x62},
	{0x31, 0xbb},
	{0x32, 0x96},
	{0x33, 0xc0},
	{0x35, 0x25},
	{0x39, 0x84},
	END_MARKER
};

static struct tp2825_state _state;

static int brightness_tbl[12] =
	{30, -15, -12, -9, -6, -3, 0, 10, 20, 30, 35, 40};

/**
 * util functions
 */
static inline struct tp2825_state *ctrl_to_me(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct tp2825_state, handler);
}

#define THINE_I2C_RETRY_CNT				3

static int _i2c_read_byte(struct i2c_client *client, u8 addr, u8 *data)
{
	struct i2c_msg msg[2];
	s8 i;
	int ret;
	u8 buf= 0;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &addr;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &buf;

	for (i = 0; i < THINE_I2C_RETRY_CNT; i++) {
		ret = i2c_transfer(client->adapter, msg, 2);
		if (likely(ret == 2))
			break;
	}

	if (unlikely(ret != 2)) {
		dev_err(&client->dev, "i2c_read_byte failed reg:0x%02x\n",
			addr);
		return -EIO;
	}

	*data = buf;
	return 0;
}

static int _i2c_write_byte(struct i2c_client *client, u8 addr, u8 val)
{
	struct i2c_msg msg;
	s8 i;
	int ret;
	u8 buf[2];

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;

	buf[0] = addr;
	buf[1] = val;

	for (i = 0; i < THINE_I2C_RETRY_CNT; i++) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (likely(ret == 1))
			break;
	}

	if (ret != 1) {
		pr_err("%s: failed to write addr 0x%x, val 0x%x\n",
		__func__, addr, val);
		return -EIO;
	}

	return 0;
}

#define V4L2_CID_MUX        (V4L2_CTRL_CLASS_USER | 0x1001)
#define V4L2_CID_STATUS     (V4L2_CTRL_CLASS_USER | 0x1002)

static int tp2825_set_mux(struct v4l2_ctrl *ctrl)
{
	struct tp2825_state *me = ctrl_to_me(ctrl);

	if (ctrl->val >= 0  && ctrl->val <= 7) {
		if (brightness_tbl[me->brightness] !=
				DEFAULT_BRIGHTNESS)
			_i2c_write_byte(me->i2c_client, 0x10,
				DEFAULT_BRIGHTNESS);
		_i2c_write_byte(me->i2c_client, 0x41, ctrl->val);

		return 0;
	}

	return -EINVAL;
}

static int tp2825_set_brightness(struct v4l2_ctrl *ctrl)
{
	struct tp2825_state *me = ctrl_to_me(ctrl);

#ifdef BRIGHTNESS_TEST
	pr_err("%s: brightness = %d\n", __func__, ctrl->val);
	_i2c_write_byte(me->i2c_client, 0x10, ctrl->val);
#else
	if (ctrl->val != me->brightness) {
		pr_err("%s: brightness = %d\n", __func__,
			brightness_tbl[ctrl->val]);
		_i2c_write_byte(me->i2c_client, 0x10,
			brightness_tbl[ctrl->val]);

		me->brightness = ctrl->val;
	}
#endif
	return 0;
}

static bool _is_camera_on(void);

static int tp2825_get_status(struct v4l2_ctrl *ctrl)
{
	struct tp2825_state *me = ctrl_to_me(ctrl);
	u8 data = 0;
	u8 mux;
	u8 val = 0;

	_i2c_read_byte(me->i2c_client, 0x41, &data);
	mux = (data & 0x07);

	if (mux >= 0 && mux <= 7) {
		_i2c_read_byte(me->i2c_client, 0x01, &data);
		if (!(data & 0x80))
			val |= 1 << mux;
	}

	ctrl->val = val;

	return 0;
}

static int tp2825_s_ctrl(struct v4l2_ctrl *ctrl)
{
	switch (ctrl->id) {
	case V4L2_CID_MUX:
		return tp2825_set_mux(ctrl);
	case V4L2_CID_BRIGHTNESS:
		return tp2825_set_brightness(ctrl);
	default:
		pr_err("%s: invalid control id 0x%x\n", __func__, ctrl->id);
		return -EINVAL;
	}
}

static int tp2825_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	switch (ctrl->id) {
	case V4L2_CID_STATUS:
		return tp2825_get_status(ctrl);
	default:
		pr_err("%s: invalid control id 0x%x\n", __func__, ctrl->id);
		return -EINVAL;
	}
}

static const struct v4l2_ctrl_ops tp2825_ctrl_ops = {
	.s_ctrl = tp2825_s_ctrl,
	.g_volatile_ctrl = tp2825_g_volatile_ctrl,
};

static const struct v4l2_ctrl_config tp2825_custom_ctrls[] = {
	{
		.ops  = &tp2825_ctrl_ops,
		.id   = V4L2_CID_MUX,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "MuxControl",
		.min  = 0,
		.max  = 1,
		.def  = 1,
		.step = 1,
	},
	{
		.ops  = &tp2825_ctrl_ops,
		.id   = V4L2_CID_STATUS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Status",
		.min  = 0,
		.max  = 1,
		.def  = 1,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	}
};

#define NUM_CTRLS 3
static int tp2825_initialize_ctrls(struct tp2825_state *me)
{
	v4l2_ctrl_handler_init(&me->handler, NUM_CTRLS);

	me->ctrl_mux = v4l2_ctrl_new_custom(&me->handler,
		&tp2825_custom_ctrls[0], NULL);
	if (!me->ctrl_mux) {
		pr_err("%s: failed to v4l2_ctrl_new_custom for mux\n",
			__func__);
		return -ENOENT;
	}

	me->ctrl_status = v4l2_ctrl_new_custom(&me->handler,
		&tp2825_custom_ctrls[1], NULL);
	if (!me->ctrl_status) {
		pr_err("%s: failed to v4l2_ctrl_new_custom for status\n",
			__func__);
		return -ENOENT;
	}

	me->ctrl_brightness = v4l2_ctrl_new_std(&me->handler, &tp2825_ctrl_ops,
			V4L2_CID_BRIGHTNESS, -128, 127, 1, 0x1e);
	if (!me->ctrl_brightness) {
		pr_err("%s: failed to v4l2_ctrl_new_std for brightness\n",
			__func__);
		return -ENOENT;
	}

	me->sd.ctrl_handler = &me->handler;
	if (me->handler.error) {
		pr_err("%s: ctrl handler error(%d)\n", __func__,
			me->handler.error);
		v4l2_ctrl_handler_free(&me->handler);
		return -EINVAL;
	}

	return 0;
}

static inline bool _is_camera_on(void)
{
	/* read status */
	u8 data;

	_i2c_read_byte(_state.i2c_client, 0x01, &data);

	if (data & 0x80)
		return false;

	if ((data & 0x40) && (data & 0x08))
		return true;

	return false;
}

static int tp2825_s_stream(struct v4l2_subdev *sd, int enable)
{
	if (enable) {
		/*	if (_state.first) {	*/
		if (1) {
			int  i = 0;
			struct tp2825_state *me = &_state;
			struct reg_val *reg_val = _sensor_init_data;

			while (reg_val->reg != 0xff) {
				_i2c_write_byte(me->i2c_client, reg_val->reg,
					reg_val->val);
				mdelay(10);
				i++;
				reg_val++;
			}
			_state.first = false;
		}
	}

	return 0;
}

static int tp2825_s_fmt(struct v4l2_subdev *sd,
		/* struct v4l2_subdev_fh *fh, */
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_format *fmt)
{
	vmsg("%s\n", __func__);
	return 0;
}

static int tp2825_s_power(struct v4l2_subdev *sd, int on)
{
	vmsg("%s: %d\n", __func__, on);
	return 0;
}

static const struct v4l2_subdev_core_ops tp2825_subdev_core_ops = {
	.s_power = tp2825_s_power,
};

static const struct v4l2_subdev_pad_ops tp2825_subdev_pad_ops = {
	.set_fmt = tp2825_s_fmt,
};

static const struct v4l2_subdev_video_ops tp2825_subdev_video_ops = {
	.s_stream = tp2825_s_stream,
};

static const struct v4l2_subdev_ops tp2825_ops = {
	.core  = &tp2825_subdev_core_ops,
	.video = &tp2825_subdev_video_ops,
	.pad   = &tp2825_subdev_pad_ops,
};

static int tp2825_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct tp2825_state *state = &_state;
	int ret;

	vmsg("%s entered\n", __func__);

	sd = &state->sd;
	strcpy(sd->name, "tp2825");

	v4l2_i2c_subdev_init(sd, client, &tp2825_ops);

	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	state->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &state->pad);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to media_entity_init()\n",
			__func__);
		return ret;
	}

	ret = tp2825_initialize_ctrls(state);
	if (ret < 0) {
		pr_err("%s: failed to initialize controls\n",
			__func__);
		return ret;
	}

	i2c_set_clientdata(client, sd);
	state->i2c_client = client;
	state->first = true;

	vmsg("%s exit\n", __func__);

	return 0;
}

static int tp2825_remove(struct i2c_client *client)
{
	struct tp2825_state *state = &_state;

	v4l2_device_unregister_subdev(&state->sd);
	return 0;
}

static const struct i2c_device_id tp2825_id[] = {
	{ "tp2825", 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, tp2825_id);

static struct i2c_driver tp2825_i2c_driver = {
	.driver = {
		.name = "tp2825",
	},
	.probe = tp2825_probe,
	.remove = tp2825_remove,
	.id_table = tp2825_id,
};

module_i2c_driver(tp2825_i2c_driver);

MODULE_DESCRIPTION("TP2825 Camera Sensor Driver");
MODULE_AUTHOR("<jkchoi@nexell.co.kr>");
MODULE_LICENSE("GPL");
