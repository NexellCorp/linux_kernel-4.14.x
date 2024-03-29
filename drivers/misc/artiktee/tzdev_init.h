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

#ifndef __SOURCE_TZDEV_INIT_H__
#define __SOURCE_TZDEV_INIT_H__

void tzsys_init(void);
void tzmem_init(void);
void tzio_link_init(void);

int nsrpc_init_early(void);
int nsrpc_init(void);

#endif /* __SOURCE_TZDEV_INIT_H__ */
