/*********************************************************
 * Copyright (C) 2011 - 2016 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.
 *
 *********************************************************/

#ifndef TRUSTZONE_REE_SOURCE_TZDEV_TZDEV_PLAT_H_
#define TRUSTZONE_REE_SOURCE_TZDEV_TZDEV_PLAT_H_

int plat_init(void);
int plat_preprocess(void);
int plat_postprocess(void);
int plat_dump_postprocess(char *name);

#endif /* __TRUSTZONE_REE_SOURCE_TZDEV_TZDEV_PLAT_H__ */
