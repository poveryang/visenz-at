
#if defined (__UITRON) || defined (__ECOS)

#inc <malloc.h>
#inc <stdio.h>
#inc <string.h>
#inc "Type.h"
#inc "kernel.h"

#elif defined (__FREERTOS )

#inc <stdio.h>
#inc <string.h>
#inc <stdlib.h>

#inc <kwrap/file.h>

#inc <malloc.h>
#inc "kwrap/type.h"
#inc "kwrap/platform.h"
#inc "kwrap/util.h"

extern void *pvPortMalloc(size_t xWantedSize);
extern void vPortFree(void *pv);

#elif defined (__KERNEL__) || defined (__LINUX)
#inc <linux/string.h>
#inc <linux/printk.h>
#inc <linux/kernel.h>
#inc <linux/slab.h>
#inc <linux/fs.h>
#inc <linux/uaccess.h>
//#inc <comm/nvtmem.h>


#inc "kwrap/type.h"
#inc "kwrap/platform.h"
#inc "kwrap/util.h"
#inc <kwrap/file.h>

#inc "mach/rcw_macro.h"
#inc "mach/nvt-io.h"
#inc "kwrap/type.h"
#inc <plat-na51055/nvt-sramctl.h>
//#inc <frammap/frammap_if.h>
#inc <mach/fmem.h>

#inc "vpe_drv.h"

#endif

//#inc "vpe_api.h"
#include "vpe_dbg.h"

//#inc "kdrv_videoprocess/kdrv_ipp_utility.h"
//#inc "kdrv_videoprocess/kdrv_ipp_config.h"
//#inc "kdrv_videoprocess/kdrv_vpe.h"
//#inc "kdrv_videoprocess/kdrv_ipp.h"

#include "vpe_platform.h"
#include "vpe_kit.h"
#include "vpe_int.h"


//-------------------------------------------------------

#define VPE_TOOL_KIT_EN    0




#if (VPE_TOOL_KIT_EN == 1)

static UINT32 nvt_vpe_fmd_flag = 0;
static UINT32 nvt_vpe_lld_flag = 0;

INT32 nvt_vpe_isr_cb(UINT32 handle, UINT32 sts, void *p_in_data, void *p_out_data)
{

	if (sts & KDRV_VPE_INTS_FRM_END) {
		nvt_vpe_fmd_flag = 1;
	}

	if (sts & KDRV_VPE_INTS_LL_END) {
		nvt_vpe_lld_flag = 1;
	}

	return 0;
}
#endif





#if defined (__LINUX) || defined (__KERNEL__)

int nvt_vpe_api_dump(PVPE_INFO pvpe_info, unsigned char argc, char **pargv)
{
	DBG_IND("Dump kdriver\r\n");
	//kdrv_vpe_dump_info(printk);

	debug_dumpmem(0xf0cd0000, 0x1200);

	return 0;
}


int nvt_kdrv_vpe_test(PVPE_INFO pvpe_info, unsigned char argc, char **pargv)
{
#if (VPE_TOOL_KIT_EN == 1)
	KDRV_VPE_OPENCFG vpe_open_obj = {0};
	UINT32 id = 0;

	id = KDRV_DEV_ID(KDRV_CHIP0, KDRV_VIDEOPROCS_VPE_ENGINE0, 0);
	kdrv_vpe_open(id, KDRV_VIDEOPROCS_VPE_ENGINE0);

	vpe_open_obj.vpe_clock_sel = 240;
	kdrv_vpe_set(id, KDRV_VPE_PARAM_IPL_OPENCFG, (void *)&vpe_open_obj);

	kdrv_vpe_set(id, KDRV_VPE_PARAM_IPL_ISR_CB, (void *)nvt_vpe_isr_cb);


#else
	DBG_ERR("do not support\r\n");
	return 0;
#endif

}


int nvt_vpe_fw_power_saving(PVPE_INFO pmodule_info, unsigned char argc, char **pargv)
{
	unsigned int enable = 1;

	if (argc > 0) {
		if (kstrtoint(pargv[0], 0, &enable)) {
			DBG_ERR("Fail to transform string %s to unsigned int from parameter!\r\n", pargv[0]);
			return 1;
		}
	} else {
		DBG_WRN("User should pass 1 parameter!\r\n");
		return 1;
	}

	if (enable == 1) {
		fw_vpe_power_saving_en = TRUE;
	} else {
		fw_vpe_power_saving_en = FALSE;
	}

	DBG_IND("end\r\n");

	return 0;
}



#elif defined (__UITRON) || defined (__ECOS) || defined (__FREERTOS)

int nvt_vpe_api_dump(unsigned char argc, char **pargv)
{
	DBG_IND("Dump kdriver\r\n");
	//kdrv_vpe_dump_info(printf);

	return 0;
}


int nvt_kdrv_vpe_test(unsigned char argc, char **pargv)
{
#if (VPE_TOOL_KIT_EN == 1)
	DBG_IND("do not support\r\n");
	return 0;
#else
	DBG_IND("do not support\r\n");
	return 0;
#endif
}


#else
#endif





