/**
	@brief Source file of vendor net flow sample.

	@file net_flow_platform.c

	@ingroup net_flow_platform

	@note Nothing.

	Copyright Novatek Microelectronics Corp. 2018.  All rights reserved.
*/

/*-----------------------------------------------------------------------------*/
/* Include Files                                                               */
/*-----------------------------------------------------------------------------*/
#include "kwrap/cpu.h"
#include "net_flow_platform.h"
#include "../../../k_driver/include/kdrv_ai_version.h"


#if defined(__LINUX)
#inc <linux/module.h>
#inc <linux/moduleparam.h>
#inc <linux/platform_device.h>
#inc <linux/string.h>
#inc "mach/fmem.h"
#inc <asm/io.h>  /* for ioremap and iounmap */
#inc <plat-na51055/top.h>
#inc "kflow_common/nvtmpp.h"
#elif defined(__FREERTOS)
#inc "rtos_na51055/nvt-sramctl.h"
#inc "rtos_na51055/top.h"
#inc <string.h>
#inc <stdlib.h>
#inc <stdio.h>
#else
#endif


UINT32 nvt_ai_pa2va_remap(UINT32 pa, UINT32 sz)
{
	UINT32 va = 0;
#if defined(__LINUX)
	if (sz == 0) {
		return va;
	}
	if (pfn_valid(__phys_to_pfn(pa))) {
		va = (UINT32)__va(pa);
	} else {
		va = (UINT32)nvtmpp_sys_pa2va(pa);
		if (va == 0) {
			va = (UINT32)ioremap(pa, PAGE_ALIGN(sz));
		}
	}
#else
	va = pa;
#endif
	if (va > 0) {
		vos_cpu_dcache_sync(va, sz, VOS_DMA_TO_DEVICE);
	}
	
	return va;
}

UINT32 nvt_ai_pa2va_remap_wo_sync(UINT32 pa, UINT32 sz)
{
	UINT32 va = 0;
#if defined(__LINUX)
	if (sz == 0) {
		return va;
	}
	if (pfn_valid(__phys_to_pfn(pa))) {
		va = (UINT32)__va(pa);
	} else {
		va = (UINT32)nvtmpp_sys_pa2va(pa);
		if (va == 0) {
			va = (UINT32)ioremap(pa, PAGE_ALIGN(sz));
		}
	}
#else
	va = pa;
#endif

	return va;
}

VOID nvt_ai_pa2va_unmap(UINT32 va, UINT32 pa)
{
	if (va == 0) {
		return;
	}
#if defined(__LINUX)
	if (!pfn_valid(__phys_to_pfn(pa))) {
		UINT32 tmp_va = (UINT32)nvtmpp_sys_pa2va(pa);
		if (tmp_va == 0) {
			iounmap((VOID *)va);
		}
	}
#endif
}

UINT32 nvt_ai_chk_ic_ver(VOID)
{
	UINT32 ret = 0;
	
	if (nvt_get_chip_id() == CHIP_NA51055) {
		ret = 1;
	} else {
		ret = 0;
	}
	
	return ret;
}

#if defined(__LINUX)
MODULE_AUTHOR("Novatek Corp.");
MODULE_DESCRIPTION("AI kflow");
MODULE_LICENSE("GPL");
MODULE_VERSION(KDRV_AI_IMPL_VERSION);
#endif
