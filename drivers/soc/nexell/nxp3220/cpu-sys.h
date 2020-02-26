/*
 * Copyright (c) 2019 Nexell Co., Ltd.
 * Author: Ken Kim <kenkim@nexell.co.kr>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef __CPU_SYS_H
#define __CPU_SYS_H

int nx_cpu_id_ecid(u32 ecid[4]);
int nx_cpu_hpm_ro(u16 hpm[8]);
int read_cpu_hpm(void);

#endif /* __CPU_SYS_H */
