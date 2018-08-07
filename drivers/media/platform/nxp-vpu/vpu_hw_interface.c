/*
 * Copyright (C) 2016  Nexell Co., Ltd.
 * Author: Seonghee, Kim <kshblue@nexell.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <asm/io.h>
#include "vpu_hw_interface.h"

#define DBG_VBS 0

static void *gstBaseAddr;


/*----------------------------------------------------------------------------
 *	Register Interface
 */

void VpuWriteReg(uint32_t offset, uint32_t value)
{
	uint32_t *addr = (uint32_t*)((void*)(gstBaseAddr+offset));
	writel(value, addr);
#if 1
	NX_DbgMsg(NX_REG_EN_MSG, "wr(0x%p, 0x%08x, 0x%08x)\n",
		addr, offset, value);
#else
	NX_DbgMsg(NX_REG_EN_MSG, "wr(0x%p, 0x%08x, 0x%08x) verify(0x%08x)%s\n",
		addr, offset, value, readl(addr),
		(value != readl(addr)) ? " Mismatch" : "");
#endif
}

uint32_t VpuReadReg(uint32_t offset)
{
	uint32_t *addr = (uint32_t*)((void*)(gstBaseAddr+offset));
	NX_DbgMsg(NX_REG_EN_MSG, "rd(0x%p, 0x%08x, 0x%08x)\n",
		addr, offset, readl(addr));
	return readl(addr);
}

void VpuWriteRegNoMsg(uint32_t offset, uint32_t value)
{
	uint32_t *addr = (uint32_t*)((void*)(gstBaseAddr+offset));
	writel(value, addr);
}

uint32_t VpuReadRegNoMsg(uint32_t offset)
{
	uint32_t *addr = (uint32_t*)((void*)(gstBaseAddr+offset));
	return readl(addr);
}

void WriteReg32(uint32_t *address, uint32_t value)
{
	writel(value, address);
}

uint32_t ReadReg32(uint32_t *address)
{
	return readl(address);
}

void InitVpuRegister(void *virAddr)
{
	gstBaseAddr = virAddr;
}

uint32_t *GetVpuRegBase(void)
{
	return gstBaseAddr;
}

/*----------------------------------------------------------------------------
 *		Host Command
 */
void VpuBitIssueCommand(struct nx_vpu_codec_inst *inst, enum nx_vpu_cmd cmd)
{
	NX_DbgMsg(DBG_VBS, "VpuBitIssueCommand : cmd = %d, address=0x%llx, ",
		cmd, inst->instBufPhyAddr);
	NX_DbgMsg(DBG_VBS, "instIndex=%d, codecMode=%d, auxMode=%d\n",
		inst->instIndex, inst->codecMode, inst->auxMode);

	VpuWriteReg(BIT_WORK_BUF_ADDR, (uint32_t)inst->instBufPhyAddr);
	VpuWriteReg(BIT_BUSY_FLAG, 1);
	VpuWriteReg(BIT_RUN_INDEX, inst->instIndex);
	VpuWriteReg(BIT_RUN_COD_STD, inst->codecMode);
	VpuWriteReg(BIT_RUN_AUX_STD, inst->auxMode);
	VpuWriteReg(BIT_RUN_COMMAND, cmd);
}
