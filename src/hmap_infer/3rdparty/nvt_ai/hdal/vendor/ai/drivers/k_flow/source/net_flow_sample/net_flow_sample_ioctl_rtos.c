/**
	@brief Source file of IO control of vendor net flow sample.

	@file net_flow_sample_ioctl.c

	@ingroup net_flow_sample

	@note Nothing.

	Copyright Novatek Microelectronics Corp. 2018.  All rights reserved.
*/

/*-----------------------------------------------------------------------------*/
/* Include Files                                                               */
/*-----------------------------------------------------------------------------*/




#if defined(__LINUX)
#inc <linux/delay.h>
#inc <linux/string.h>
#inc "kwrap/debug.h"
#if defined (_BSP_NA51000_)
#inc "kwrap/stdio.h"
#elif defined (_BSP_NA51055_)
#inc "kwrap/util.h"
#endif
#elif defined(__FREERTOS)
//#inc <stdio.h>
//#inc <stdlib.h>
#inc <string.h>
//#inc "kwrap/util.h"
#inc "kwrap/debug.h"

#elif defined(__UITRON) || defined(__ECOS)
#inc <stdio.h>
#inc "DebugModule.h"
#inc "Delay.h"
#endif


//#inc <linux/delay.h>
//#inc <linux/module.h>
//#inc <linux/ioctl.h>
//#inc <linux/miscdevice.h>
//#inc <linux/proc_fs.h>
//#inc <linux/uaccess.h>
//#inc "kwrap/type.h"
//#inc "frammap/frammap_if.h"
//#inc "mach/fmem.h"
#include "net_flow_sample/net_flow_sample.h"
//#inc "net_flow_sample_ioctl.h"
//#inc "net_flow_sample_parm.h"
#include "net_flow_sample_int.h"
//#inc "net_flow_sample_dbg.h"
//#inc "net_flow_sample/nn_diff.h"

/*-----------------------------------------------------------------------------*/
/* Macro Constant Definitions                                                  */
/*-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------*/
/* Local Global Variables                                                      */
/*-----------------------------------------------------------------------------*/
//static vendor_ais_flow_dev_t vendor_ais_flow_dev;
extern VENDOR_AIS_FLOW_MAP_MEM_PARM g_ai_map_mem;

//static void *vendor_ais_buf_va_addr = NULL;     //???!! from structure changed to void

/*-----------------------------------------------------------------------------*/
/* Local Functions                                                             */
/*-----------------------------------------------------------------------------*/
//static int vendor_ais_flow_miscdev_open(struct inode *inode, struct file *file)
//{
//	int ret = 0;
//	//vendor_ais_flow_dev_t *pdev = NULL;
//	///* lookup device */
//	//if (vendor_ais_flow_dev.miscdev.minor == iminor(inode)) {
//	//	pdev = &vendor_ais_flow_dev;
//	//}
//
//	//if (!pdev) {
//	//	ret = -EINVAL;
//	//	goto exit;
//	//}
//
//	//file->private_data = (void *)pdev;
//
//	////vendor_ais_flow_init_mem();
//
//exit:
//	return ret;
//}
//
//static int vendor_ais_flow_miscdev_release(struct inode *inode, struct file *file)
//{
//	//if (vendor_ais_buf_va_addr) {
//	//	if (fmem_release_from_cma(vendor_ais_buf_va_addr, 0) < 0) {
//	//		return -1;
//	//	}
//	//	vendor_ais_buf_va_addr = NULL;
//	//}
//
//	return 0;
//}

int vendor_ais_flow_miscdev_ioctl(int fd, unsigned int uiCmd, void *p_arg)
{
	int iRet = 0;

	VENDOR_AIS_FLOW_MAP_MEM_INFO map_mem_info;
	VENDOR_AIS_FLOW_PROC_INFO proc_info;
	VENDOR_AIS_FLOW_PROC_INPUT_INFO input_info;
	VENDOR_AIS_FLOW_UPDATE_INFO up_info;
	VENDOR_AIS_FLOW_UPDATE_NET_INFO up_net_info;


	nvt_dbg(IND, "cmd:%x\n", uiCmd);

	switch (uiCmd) {
	/* Add other operations here */
	case VENDOR_AIS_FLOW_IOC_REMAP_ADDR:
		memcpy(&map_mem_info, p_arg, sizeof(VENDOR_AIS_FLOW_MAP_MEM_INFO));	
		nvt_ai_init_mem(&map_mem_info.parm, map_mem_info.net_id);
		break;

	case VENDOR_AIS_FLOW_IOC_UNMAP_ADDR:
		memcpy(&map_mem_info, p_arg, sizeof(VENDOR_AIS_FLOW_MAP_MEM_INFO));	
		nvt_ai_uninit_mem(map_mem_info.net_id);
		break;

	case VENDOR_AIS_FLOW_IOC_PARS_MODEL:
		memcpy(&map_mem_info, p_arg, sizeof(VENDOR_AIS_FLOW_MAP_MEM_INFO));	
		nvt_ai_pars_net(&map_mem_info.parm, map_mem_info.net_id);
		break;
		
#if !CNN_25_MATLAB
	case VENDOR_AIS_FLOW_IOC_UNPARS_MODEL:
		memcpy(&map_mem_info, p_arg, sizeof(VENDOR_AIS_FLOW_MAP_MEM_INFO));	
		nvt_ai_unpars_net(&map_mem_info.parm, map_mem_info.net_id);
		break;
#endif	

	case VENDOR_AIS_FLOW_IOC_SWITCH_PROC:
		memcpy(&proc_info, p_arg, sizeof(VENDOR_AIS_FLOW_PROC_INFO));
		nvt_ai_proc_net(&proc_info.parm, proc_info.net_id);
		break;

	case VENDOR_AIS_FLOW_IOC_INPUT_INIT:
		memcpy(&input_info, p_arg, sizeof(VENDOR_AIS_FLOW_PROC_INPUT_INFO));
#if CNN_25_MATLAB
		nvt_ai_proc_input_init(input_info.net_addr, &input_info.iomem, input_info.net_id);
#else	
		nvt_ai_proc_input_init(input_info.net_addr, input_info.imem, NN_IMEM_NUM, input_info.net_id);
#endif
		break;

	case VENDOR_AIS_FLOW_IOC_INPUT_UNINIT:
		memcpy(&input_info, p_arg, sizeof(VENDOR_AIS_FLOW_PROC_INPUT_INFO));
		nvt_ai_proc_input_uninit(input_info.net_addr, input_info.net_id);
		break;

	case VENDOR_AIS_FLOW_IOC_UPDATE_PARM:
		memcpy(&up_info, p_arg, sizeof(VENDOR_AIS_FLOW_UPDATE_INFO));
		nvt_ai_update_layer(up_info.layer, up_info.net_id);
		break;
	case VENDOR_AIS_FLOW_IOC_PROC_UPDATE_INFO:
		memcpy(&up_net_info, p_arg, sizeof(VENDOR_AIS_FLOW_UPDATE_NET_INFO));
		nvt_ai_update_net_online(&up_net_info.map_parm, (NN_DIFF_MODEL_HEAD *)up_net_info.net_info_va, up_net_info.model_id, up_net_info.net_id);
		break;
		
	case VENDOR_AIS_FLOW_IOC_RESTORE_UPDATE_INFO:
		memcpy(&up_net_info, p_arg, sizeof(VENDOR_AIS_FLOW_UPDATE_NET_INFO));
		nvt_ai_restore_net_online(&up_net_info.map_parm, (NN_DIFF_MODEL_HEAD *)up_net_info.net_info_va, up_net_info.model_id, up_net_info.net_id);
		break;
	default :
		break;
	}
//exit:
	return iRet;
}