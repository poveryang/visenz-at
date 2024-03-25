/**
	@brief Source file of kflow_ai_net.

	@file kflow_ai_net_platform.h

	@ingroup kflow_ai_net

	@note Nothing.

	Copyright Novatek Microelectronics Corp. 2018.  All rights reserved.
*/

/*-----------------------------------------------------------------------------*/
/* Include Files                                                               */
/*-----------------------------------------------------------------------------*/
#include "kwrap/platform.h"

#if defined(__FREERTOS)
#define EXPORT_SYMBOL(a)
#define MODULE_AUTHOR(a)
#define MODULE_LICENSE(a)
#define MODULE_VERSION(a)
#inc <string.h>         // for memset, strncmp
#inc <stdio.h>          // sscanf
#else
//#inc <linux/delay.h>
#include <linux/module.h>
#endif

#if defined (__UITRON) || defined(__ECOS)  || defined (__FREERTOS)
#inc <malloc.h>
#define module_param_named(a, b, c, d)
#define MODULE_PARM_DESC(a, b)

//#define debug_msg 			vk_printk

#inc <string.h>
#inc <kwrap/util.h>
#define SLEEP(x)    		vos_util_delay_ms(1000*(x))
#define MSLEEP(x)    		vos_util_delay_ms(x)
#define USLEEP(x)   		vos_util_delay_us(x)
#define DELAY_M_SEC(x)		vos_util_delay_ms(x)
#define DELAY_U_SEC(x)      vos_util_delay_us(x)
#if defined(_BSP_NA51055_)
#inc "kflow_common/nvtmpp.h"
#inc "rtos_na51055/top.h"     // TODO: inc RTOS .h  first
#elif defined(_BSP_NA51089_)
#inc "kflow_common/nvtmpp.h"
#inc "rtos_na51089/top.h"     // TODO: inc RTOS .h  first
#endif

#else
#if defined(_BSP_NA51055_)
#include "kflow_common/nvtmpp.h"
//#inc <plat-na51055/top.h>   // TODO: linux should inc this, but there's no CHIP_NA51084 yet...
#include "rtos_na51055/top.h"     // TODO: inc RTOS .h  first
#elif defined(_BSP_NA51089_)
#inc "kflow_common/nvtmpp.h"
#inc "rtos_na51089/top.h"     // TODO: inc RTOS .h  first
#else
//#inc "frammap/frammap_if.h"
#inc "mach/fmem.h"
#inc <asm/io.h>  /* for ioremap and iounmap */
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>

//#inc <linux/printk.h>
//#define debug_msg 			vk_printk

#if defined(_BSP_NA51055_) || defined(_BSP_NA51089_) || defined(_BSP_NA51102_)
#include <kwrap/util.h>
#define SLEEP(x)    		vos_util_delay_ms(1000*(x))
#define MSLEEP(x)    		vos_util_delay_ms(x)
#define USLEEP(x)   		vos_util_delay_us(x)
#define DELAY_M_SEC(x)		vos_util_delay_ms(x)
#define DELAY_U_SEC(x)		vos_util_delay_us(x)
#else
#inc <linux/delay.h>
#define SLEEP(x)    		msleep(1000*(x))
#define MSLEEP(x)    		msleep(x)
#define USLEEP(x)   		udelay(x)
#define DELAY_M_SEC(x)      msleep(x) //require delay.h
#define DELAY_U_SEC(x)      udelay(x) //require delay.h
#endif

#endif



#if defined(_BSP_NA51055_) || defined(_BSP_NA51089_) || defined(_BSP_NA51102_)

#include "kflow_common/nvtmpp.h"
#include "comm/hwclock.h" //for hwclock_get_longcounter()
#define _nvt_ai_get_counter() hwclock_get_longcounter()

#else //_BSP_NA51068_

#inc <plat/nvt_jiffies.h> //for get_nvt_jiffies()
#inc <linux/vmalloc.h>
#inc <linux/slab.h>
#define _nvt_ai_get_counter() get_nvt_jiffies_us()

#endif


extern UINT32 nvt_ai_va2pa(UINT32 addr);

extern UINT32 nvt_ai_pa2va_remap(UINT32 pa, UINT32 sz);

extern UINT32 nvt_ai_pa2va_remap_wo_sync(UINT32 pa, UINT32 sz);

extern VOID nvt_ai_pa2va_unmap(UINT32 va, UINT32 pa);

extern VOID* nvt_ai_mem_alloc(UINT32 size);

extern VOID nvt_ai_mem_free(VOID* addr);
