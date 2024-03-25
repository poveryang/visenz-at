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
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include "kwrap/type.h"
//#inc "frammap/frammap_if.h"
#include "mach/fmem.h"
#include "net_flow_sample/net_flow_sample.h"
#include "net_flow_sample_ioctl.h"
#include "net_flow_sample_parm.h"
#include "net_flow_sample_int.h"
#include "net_flow_sample_dbg.h"
#include "net_flow_sample/nn_diff.h"
#include "net_flow_platform.h"
#include "net_flow_ai_version.h"
#include <kwrap/semaphore.h>
#include "kdrv_ai.h"
/*-----------------------------------------------------------------------------*/
/* Macro Constant Definitions                                                  */
/*-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------*/
/* Local Global Variables                                                      */
/*-----------------------------------------------------------------------------*/
#if DEV_UPDATE
static NETFLOW_DRV_INFO m_pdrv_info;
#endif
static vendor_ais_flow_dev_t vendor_ais_flow_dev;
extern VENDOR_AIS_FLOW_MAP_MEM_PARM g_ai_map_mem;

static void *vendor_ais_buf_va_addr = NULL;     //???!! from structure changed to void

extern int kdrv_ai_do_vmalloc_for_netflowsample(VOID);
extern VOID kdrv_ai_do_vfree_for_netflowsample(VOID);
static UINT32 ai_flow_is_init = 0;

SEM_HANDLE ai_sem_net_id = 0;
static BOOL g_ai_net_is_used[AI_SUPPORT_NET_MAX] = {0};
extern UINT32 kdrv_ai_drv_get_net_supported_num(VOID);

/*-----------------------------------------------------------------------------*/
/* Local Functions                                                             */
/*-----------------------------------------------------------------------------*/
static int vendor_ais_flow_miscdev_open(struct inode *inode, struct file *file)
{
	int ret = 0;
#if DEV_UPDATE
	NETFLOW_DRV_INFO* pdev = &m_pdrv_info;
#else
	vendor_ais_flow_dev_t *pdev = NULL;
	/* lookup device */
	if (vendor_ais_flow_dev.miscdev.minor == iminor(inode)) {
		pdev = &vendor_ais_flow_dev;
	}

	if (!pdev) {
		ret = -EINVAL;
		goto exit;
	}
#endif

	file->private_data = (void *)pdev;

	//vendor_ais_flow_init_mem();

#if !DEV_UPDATE
exit:
#endif
	return ret;
}

static long vendor_ais_flow_miscdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	VENDOR_AIS_FLOW_MAP_MEM_INFO map_mem_info = {0};
	VENDOR_AIS_FLOW_PROC_INFO proc_info;
	VENDOR_AIS_FLOW_PROC_INPUT_INFO input_info;
	VENDOR_AIS_FLOW_UPDATE_INFO up_info;
	VENDOR_AIS_FLOW_UPDATE_NET_INFO up_net_info;
	VENDOR_AIS_FLOW_PROC_INPUT_LAYER_INFO input_layer_info;
	UINT32 valid_net_id = 0;
	UINT32 i = 0;


	if (_IOC_TYPE(cmd) != VENDOR_AIS_FLOW_IOC_MAGIC) {
		ret = -ENOIOCTLCMD;
		goto exit;
	}

	switch (cmd) {
	case VENDOR_AIS_FLOW_IOC_REMAP_ADDR:
		if (copy_from_user(&map_mem_info, (void __user *)arg, sizeof(VENDOR_AIS_FLOW_MAP_MEM_INFO))) {
			ret = -EFAULT;
			goto exit;
		}
		nvt_ai_init_mem(&map_mem_info.parm, map_mem_info.net_id);
		ret = (copy_to_user((void __user *)arg, &map_mem_info, sizeof(VENDOR_AIS_FLOW_MAP_MEM_INFO))) ? (-EFAULT) : 0;
		break;

	case VENDOR_AIS_FLOW_IOC_UNMAP_ADDR:
		if (copy_from_user(&map_mem_info, (void __user *)arg, sizeof(VENDOR_AIS_FLOW_MAP_MEM_INFO))) {
			ret = -EFAULT;
			goto exit;
		}
		nvt_ai_uninit_mem(map_mem_info.net_id);
		break;

	case VENDOR_AIS_FLOW_IOC_PARS_MODEL:
		if (copy_from_user(&map_mem_info, (void __user *)arg, sizeof(VENDOR_AIS_FLOW_MAP_MEM_INFO))) {
			ret = -EFAULT;
			goto exit;
		}
		ret = nvt_ai_pars_net(&map_mem_info.parm, map_mem_info.net_id);
		break;
		
#if !CNN_25_MATLAB
	case VENDOR_AIS_FLOW_IOC_UNPARS_MODEL:
		if (copy_from_user(&map_mem_info, (void __user *)arg, sizeof(VENDOR_AIS_FLOW_MAP_MEM_INFO))) {
			ret = -EFAULT;
			goto exit;
		}
		nvt_ai_unpars_net(&map_mem_info.parm, map_mem_info.net_id);
		break;
#endif	

	case VENDOR_AIS_FLOW_IOC_SWITCH_PROC:
		if (copy_from_user(&proc_info, (void __user *)arg, sizeof(VENDOR_AIS_FLOW_PROC_INFO))) {
			ret = -EFAULT;
			goto exit;
		}
		nvt_ai_proc_net(&proc_info.parm, proc_info.net_id);
		ret = (copy_to_user((void __user *)arg, &proc_info, sizeof(VENDOR_AIS_FLOW_PROC_INFO))) ? (-EFAULT) : 0;
		break;

	case VENDOR_AIS_FLOW_IOC_INPUT_INIT:
		if (copy_from_user(&input_info, (void __user *)arg, sizeof(VENDOR_AIS_FLOW_PROC_INPUT_INFO))) {
			ret = -EFAULT;
			goto exit;
		}
#if CNN_25_MATLAB
		nvt_ai_proc_input_init(input_info.net_addr, &input_info.iomem, input_info.net_id);
#else	
		nvt_ai_proc_input_init(input_info.net_addr, input_info.imem, NN_IMEM_NUM, input_info.net_id);
#endif
		break;

	case VENDOR_AIS_FLOW_IOC_INPUT_UNINIT:
		if (copy_from_user(&input_info, (void __user *)arg, sizeof(VENDOR_AIS_FLOW_PROC_INPUT_INFO))) {
			ret = -EFAULT;
			goto exit;
		}
		nvt_ai_proc_input_uninit(input_info.net_addr, input_info.net_id);
		break;

	case VENDOR_AIS_FLOW_IOC_UPDATE_PARM:
		if (copy_from_user(&up_info, (void __user *)arg, sizeof(VENDOR_AIS_FLOW_UPDATE_INFO))) {
			ret = -EFAULT;
			goto exit;
		}
		ret = nvt_ai_update_layer(up_info.layer, up_info.net_id);
		break;
	case VENDOR_AIS_FLOW_IOC_PROC_UPDATE_INFO:
		if (copy_from_user(&up_net_info, (void __user *)arg, sizeof(VENDOR_AIS_FLOW_UPDATE_NET_INFO))) {
			ret = -EFAULT;
			goto exit;
		}
		up_net_info.net_info_va = (UINT32)nvt_ai_pa2va_remap(up_net_info.net_info_pa, up_net_info.net_info_size);
		nvt_ai_update_net_online(&up_net_info.map_parm, (NN_DIFF_MODEL_HEAD *)up_net_info.net_info_va, up_net_info.model_id, up_net_info.net_id);
		nvt_ai_pa2va_unmap(up_net_info.net_info_va, up_net_info.net_info_pa);
		break;
		
	case VENDOR_AIS_FLOW_IOC_RESTORE_UPDATE_INFO:
		if (copy_from_user(&up_net_info, (void __user *)arg, sizeof(VENDOR_AIS_FLOW_UPDATE_NET_INFO))) {
			ret = -EFAULT;
			goto exit;
		}
		up_net_info.net_info_va = (UINT32)nvt_ai_pa2va_remap(up_net_info.net_info_pa, up_net_info.net_info_size);
		nvt_ai_restore_net_online(&up_net_info.map_parm, (NN_DIFF_MODEL_HEAD *)up_net_info.net_info_va, up_net_info.model_id, up_net_info.net_id);
		nvt_ai_pa2va_unmap(up_net_info.net_info_va, up_net_info.net_info_pa);	
		break;
	case VENDOR_AIS_FLOW_IOC_INPUT_LAYER_INIT:
		if (copy_from_user(&input_layer_info, (void __user *)arg, sizeof(VENDOR_AIS_FLOW_PROC_INPUT_LAYER_INFO))) {
			ret = -EFAULT;
			goto exit;
		}
#if !CNN_25_MATLAB
		nvt_ai_proc_input_layer_init2(input_layer_info.net_addr, input_layer_info.imem, NN_IMEM_NUM, input_layer_info.proc_idx, input_layer_info.net_id);
#endif
		break;

	case VENDOR_AIS_FLOW_IOC_INPUT_LAYER_UNINIT:
		if (copy_from_user(&input_layer_info, (void __user *)arg, sizeof(VENDOR_AIS_FLOW_PROC_INPUT_LAYER_INFO))) {
			ret = -EFAULT;
			goto exit;
		}
#if !CNN_25_MATLAB
		nvt_ai_proc_input_layer_uninit2(input_layer_info.net_addr, input_layer_info.proc_idx, input_layer_info.net_id);
#endif
		break;
	case VENDOR_AIS_FLOW_IOC_INIT_STATUS:
		if (ai_flow_is_init == 1) {
			printk("start reset flow\n");
			nvt_ai_reset_status();
		}
		ai_flow_is_init = 1;
		break;
	case VENDOR_AIS_FLOW_IOC_UNINIT_STATUS:
		ai_flow_is_init = 0;
		break;
	case VENDOR_AIS_FLOW_IOC_GET_VER:{
		CHAR version_info[32] = KFLOW_AI_IMPL_VERSION;
		ret = copy_to_user((void __user *)arg, version_info, sizeof(KFLOW_AI_IMPL_VERSION));
	}
		break;
		
	case VENDOR_AIS_FLOW_IOC_PROC_UPDATE_BATCH_INFO:
		if (copy_from_user(&up_net_info, (void __user *)arg, sizeof(VENDOR_AIS_FLOW_UPDATE_NET_INFO))) {
			ret = -EFAULT;
			goto exit;
		}
		up_net_info.net_info_va = (UINT32)nvt_ai_pa2va_remap(up_net_info.net_info_pa, up_net_info.net_info_size);
		nvt_ai_update_net_online_batch(&up_net_info.map_parm, (NN_DIFF_MODEL_HEAD *)up_net_info.net_info_va, up_net_info.model_id, up_net_info.net_id);
		nvt_ai_pa2va_unmap(up_net_info.net_info_va, up_net_info.net_info_pa);
		break;
		
	case VENDOR_AIS_FLOW_IOC_RESTORE_UPDATE_BATCH_INFO:
		if (copy_from_user(&up_net_info, (void __user *)arg, sizeof(VENDOR_AIS_FLOW_UPDATE_NET_INFO))) {
			ret = -EFAULT;
			goto exit;
		}
		up_net_info.net_info_va = (UINT32)nvt_ai_pa2va_remap(up_net_info.net_info_pa, up_net_info.net_info_size);
		nvt_ai_restore_net_online_batch(&up_net_info.map_parm, (NN_DIFF_MODEL_HEAD *)up_net_info.net_info_va, up_net_info.model_id, up_net_info.net_id);
		nvt_ai_pa2va_unmap(up_net_info.net_info_va, up_net_info.net_info_pa);	
		break;
	
	case VENDOR_AIS_FLOW_IOC_GET_NET_ID:
		valid_net_id = 0xFFFFFFFF;
		SEM_WAIT(ai_sem_net_id);	
		for (i = 0; i < kdrv_ai_drv_get_net_supported_num(); i++) {
			if (g_ai_net_is_used[i] == 0) {
				valid_net_id = i;
				g_ai_net_is_used[i] = 1;
				break;
			}
		}
		SEM_SIGNAL(ai_sem_net_id);
		if (valid_net_id == 0xFFFFFFFF) {
			pr_err("in VENDOR_AIS_FLOW_IOC_GET_NET_ID: get idle net id fail!\n");
		}
		ret = (copy_to_user((void __user *)arg, &valid_net_id, sizeof(UINT32))) ? (-EFAULT) : 0;
		
		break;
	
	case VENDOR_AIS_FLOW_IOC_REL_NET_ID:
		if (copy_from_user(&valid_net_id, (void __user *)arg, sizeof(UINT32))) {
			ret = -EFAULT;
			goto exit;
		}
		if (valid_net_id >= AI_SUPPORT_NET_MAX) {
			pr_err("in VENDOR_AIS_FLOW_IOC_REL_NET_ID: invalid net id for release!\n");
		} else {
			SEM_WAIT(ai_sem_net_id);
			g_ai_net_is_used[valid_net_id] = 0;
			SEM_SIGNAL(ai_sem_net_id);
		}
		
		break;
		
	default :
		break;
	}

exit:

	return ret;
}

static int vendor_ais_flow_miscdev_release(struct inode *inode, struct file *file)
{
	if (vendor_ais_buf_va_addr) {
		if (fmem_release_from_cma(vendor_ais_buf_va_addr, 0) < 0) {
			return -1;
		}
		vendor_ais_buf_va_addr = NULL;
	}

	return 0;
}

#if 0
static ssize_t vendor_ais_flow_miscdev_write(struct file *file, const char __user *buf, size_t count, loff_t *offp)
{
	int ret = 0;
	int bufsize = count;
	vendor_ais_ddr_t *mem;
	vendor_ais_flow_dev_t *pdev = (vendor_ais_flow_dev_t *)file->private_data;

	if (pdev->mem_type == 0) {
		mem = &vendor_ais_noncache_buf;
		if (bufsize >= (mem->bufsize - pdev->mem_offset - *offp)) {
			nvt_dbg(IND, "skip write file, write file size > noncache size\r\n");
			goto exit;
		}
	} else {
		mem = &vendor_ais_cache_buf;
		if (bufsize >= (mem->bufsize - pdev->mem_offset - *offp)) {
			nvt_dbg(IND, "skip write file, write file size > cache size\r\n");
			goto exit;
		}
	}
	if ((pdev->mem_limitsz != 0) && (bufsize > pdev->mem_limitsz)) {
		bufsize = pdev->mem_limitsz;
	}

	if (copy_from_user((void *)(mem->va + pdev->mem_offset + *offp), (void __user *)buf, bufsize)) {
		nvt_dbg(IND, "copy from user fail\r\n");
	} else {
		ret = bufsize;
	}

	if (mem->type == ALLOC_CACHEABLE) {
		fmem_dcache_sync((mem->va + pdev->mem_offset + *offp), bufsize, DMA_TO_DEVICE);
	}

exit:

	return ret;
}

static ssize_t vendor_ais_flow_miscdev_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	int ret = 0;
	int bufsize = count;
	vendor_ais_ddr_t *mem;
	vendor_ais_flow_dev_t *pdev = (vendor_ais_flow_dev_t *)file->private_data;

	if (pdev->mem_type == 0) {
		mem = &vendor_ais_noncache_buf;
		if (bufsize >= (mem->bufsize - pdev->mem_offset - *pos)) {
			nvt_dbg(IND, "skip read mem, read mem size > noncache size\r\n");
			goto exit;
		}
	} else {
		mem = &vendor_ais_cache_buf;
		if (bufsize >= (mem->bufsize - pdev->mem_offset - *pos)) {
			nvt_dbg(IND, "skip read mem, read mem size > cache size\r\n");
			goto exit;
		}
	}

	if ((pdev->mem_limitsz != 0) && (bufsize > pdev->mem_limitsz)) {
		bufsize = pdev->mem_limitsz;
	}

	if (mem->type == ALLOC_CACHEABLE) {
		fmem_dcache_sync((mem->va + pdev->mem_offset + *pos), bufsize, DMA_FROM_DEVICE);
	}

	if (copy_to_user((void __user *)buf, (mem->va + pdev->mem_offset + *pos), bufsize)) {
		nvt_dbg(IND, "copy to user fail\r\n");
	} else {
		ret = bufsize;
	}

exit:

	return ret;
}
#endif

static struct file_operations vendor_ais_flow_miscdev_fops = {
	.owner          = THIS_MODULE,
	.open           = vendor_ais_flow_miscdev_open,
	.release        = vendor_ais_flow_miscdev_release,
	.unlocked_ioctl = vendor_ais_flow_miscdev_ioctl,
//	.write          = vendor_ais_flow_miscdev_write,
//	.read           = vendor_ais_flow_miscdev_read,
};

/*-----------------------------------------------------------------------------*/
/* Interface Functions                                                         */
/*-----------------------------------------------------------------------------*/
int vendor_ais_flow_miscdev_init(void)
{
	int ret = 0;
#if DEV_UPDATE
	unsigned char ucloop;
	NETFLOW_DRV_INFO* pdrv_info = &m_pdrv_info;
#endif
	int v_size;
	UINT32 i = 0;

	spin_lock_init(&vendor_ais_flow_dev.ioctl_lock);

	v_size = kdrv_ai_do_vmalloc_for_netflowsample();
	if (v_size <= 0) {
		pr_err("Error, vmalloc fail to allocate memory at %s.\n", __FUNCTION__);
		ret = -ENODEV;
		goto err_exit;	
	}

#if !DEV_UPDATE
	memset(&vendor_ais_flow_dev.miscdev, 0, sizeof(struct miscdevice));
	/* create device node */
	vendor_ais_flow_dev.miscdev.name  = VENDOR_AIS_FLOW_DEV_NAME;
	vendor_ais_flow_dev.miscdev.minor = MISC_DYNAMIC_MINOR;
	vendor_ais_flow_dev.miscdev.fops  = (struct file_operations *)&vendor_ais_flow_miscdev_fops;
#endif

	//vendor_ais_flow_dev.mem_type = 0;
	//vendor_ais_flow_dev.mem_offset = 0;
	//vendor_ais_flow_dev.mem_limitsz = 0;
#if DEV_UPDATE
	//Dynamic to allocate Device ID
	if (vos_alloc_chrdev_region(&pdrv_info->dev_id, MODULE_MINOR_COUNT, MODULE_NAME)) {
		pr_err("Can't get device ID\n");
		ret = -ENODEV;
        goto err_exit_and_free;
	}

	/* Register character device for the volume */
	cdev_init(&pdrv_info->cdev, &vendor_ais_flow_miscdev_fops);
	pdrv_info->cdev.owner = THIS_MODULE;

	if (cdev_add(&pdrv_info->cdev, pdrv_info->dev_id, MODULE_MINOR_COUNT)) {
		pr_err("Can't add cdev\n");
		ret = -ENODEV;
		goto FAIL_CDEV;
	}

	pdrv_info->pmodule_class = class_create(THIS_MODULE, MODULE_NAME);
	if(IS_ERR(pdrv_info->pmodule_class)) {
		pr_err("failed in creating class.\n");
		ret = -ENODEV;
		goto FAIL_CDEV;
	}

	/* register your own device in sysfs, and this will cause udev to create corresponding device node */
	for (ucloop = 0 ; ucloop < (MODULE_MINOR_COUNT ) ; ucloop++) {
		pdrv_info->pdevice[ucloop] = device_create(pdrv_info->pmodule_class, NULL
			, MKDEV(MAJOR(pdrv_info->dev_id), (ucloop + MINOR(pdrv_info->dev_id))), NULL
			, MODULE_NAME);

		if(IS_ERR(pdrv_info->pdevice[ucloop])) {

			pr_err("failed in creating device%d.\n", ucloop);

			if (ucloop == 0) {
				device_unregister(pdrv_info->pdevice[ucloop]);
			}

			ret = -ENODEV;
			goto FAIL_CLASS;
		}
	}
	
	SEM_CREATE(ai_sem_net_id, 1);
	for (i = 0; i < AI_SUPPORT_NET_MAX; i++) {
		g_ai_net_is_used[i] = 0;
	}

	return ret;

FAIL_CLASS:
	class_destroy(pdrv_info->pmodule_class);

FAIL_CDEV:
	cdev_del(&pdrv_info->cdev);
	vos_unregister_chrdev_region(pdrv_info->dev_id, MODULE_MINOR_COUNT);
#else
	ret = misc_register(&vendor_ais_flow_dev.miscdev);
	if (ret) {
		nvt_dbg(IND, "cannot register %s miscdev on (err=%d)\r\n", vendor_ais_flow_dev.miscdev.name, ret);
		vendor_ais_flow_dev.miscdev.name = 0;
	}
#endif

err_exit_and_free:

	if (v_size > 0) {
		kdrv_ai_do_vfree_for_netflowsample();
	}

err_exit:

	return ret;
}

void vendor_ais_flow_miscdev_exit(void)
{
#if DEV_UPDATE
	unsigned char ucloop;
	NETFLOW_DRV_INFO* pdrv_info = &m_pdrv_info;
	
	for (ucloop = 0 ; ucloop < (MODULE_MINOR_COUNT ) ; ucloop++)
		device_unregister(pdrv_info->pdevice[ucloop]);

	class_destroy(pdrv_info->pmodule_class);
	cdev_del(&pdrv_info->cdev);
	vos_unregister_chrdev_region(pdrv_info->dev_id, MODULE_MINOR_COUNT);
#else
	misc_deregister(&vendor_ais_flow_dev.miscdev);
#endif
	SEM_DESTROY(ai_sem_net_id);
	kdrv_ai_do_vfree_for_netflowsample();
}

module_init(vendor_ais_flow_miscdev_init);
module_exit(vendor_ais_flow_miscdev_exit);

MODULE_AUTHOR("Novatek Microelectronics Corp.");
//MODULE_LICENSE("NVT");
MODULE_LICENSE("GPL");
