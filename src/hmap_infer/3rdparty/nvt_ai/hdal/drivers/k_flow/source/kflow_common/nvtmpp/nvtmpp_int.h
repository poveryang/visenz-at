/*
    Copyright   Novatek Microelectronics Corp. 2017.  All rights reserved.

    @file       nvtmpp_int.h

    @brief      nvtmpp internal header file

    @version    V1.00.000
    @author     Novatek FW Team
    @date       2017/02/13
*/
#ifndef _NVTMPP_INT_H
#define _NVTMPP_INT_H

/*
#if defined __LINUX
#inc <linux/kernel.h>
#inc <linux/string.h>
#inc <linux/ctype.h>
#inc <linux/io.h>
#inc <linux/delay.h>
#inc <linux/time.h>
#inc "kwrap/dev.h"
#inc "kwrap/type.h"
#inc "kwrap/semaphore.h"
#inc "kwrap/task.h"
#inc "kwrap/sxcmd.h"
#inc "kwrap/stdio.h"
#inc "kwrap/spinlock.h"
#inc "kwrap/cmdsys.h"
#inc "kwrap/cpu.h"
#inc "kwrap/file.h"
#inc "kflow_common/nvtmpp.h"
#inc "kflow_common/nvtmpp_ioctl.h"
#inc "nvtmpp_init.h"
#else
#inc <stdio.h>
#inc <string.h>
#inc <ctype.h>
#inc "kwrap/type.h"
#inc "kwrap/semaphore.h"
#inc "kwrap/task.h"
#inc "kwrap/sxcmd.h"
#inc "kwrap/stdio.h"
#inc "kwrap/spinlock.h"
#inc "kwrap/cmdsys.h"
#inc "kwrap/cpu.h"
#inc "kflow_common/nvtmpp.h"
#inc "kflow_common/nvtmpp_ioctl.h"
#endif
*/

#include "kflow_common/nvtmpp.h"

#define abort(...)
#if defined __KERNEL__
#define NVTMPP_BLK_4K_ALIGN      1
#define NVTMPP_FASTBOOT_SUPPORT  1
#else
#define NVTMPP_BLK_4K_ALIGN      0
#define NVTMPP_FASTBOOT_SUPPORT  0
#endif

extern UINT32  nvtmpp_sys_va2pa(UINT32 virt_addr);
extern void    nvtmpp_vb_set_showmsg_level(UINT32 showmsg_level);
extern UINT32  nvtmpp_vb_get_showmsg_level(void);
extern void    nvtmpp_vb_set_memory_corrupt_check_en(BOOL en);
extern BOOL    nvtmpp_vb_set_dump_max_cnt(UINT32 pool_id, UINT32 dump_max_cnt);
extern void    nvtmpp_dump_err_status(int (*dump)(const char *fmt, ...));
extern BOOL    nvtmpp_vb_get_comm_pool_range(NVTMPP_DDR ddr, MEM_RANGE *p_range);

#endif

