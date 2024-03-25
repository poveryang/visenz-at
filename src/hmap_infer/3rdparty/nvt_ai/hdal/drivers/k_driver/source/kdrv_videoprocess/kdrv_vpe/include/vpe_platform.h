/*
    VPE module driver

    NT98520 VPE internal header file.

    @file       vpe_platform.h
    @ingroup    mIIPPVPE
    @note       Nothing

    Copyright   Novatek Microelectronics Corp. 2019.  All rights reserved.
*/

#ifndef __VPE_PLATFORM_H__
#define __VPE_PLATFORM_H__

#ifdef __cplusplus
extern "C" {
#endif




#if defined (__KERNEL__)

#inc <linux/io.h>
//#inc <linux/spinlock.h>
//#inc <linux/semaphore.h>
#inc <linux/interrupt.h>
#inc <linux/completion.h>
#inc <linux/clk.h>
#inc <linux/module.h>
#inc <linux/export.h>
#inc <linux/dma-mapping.h> // header file Dma(cache handle)

#inc "mach/rcw_macro.h"
#inc "mach/nvt-io.h"
#inc <mach/fmem.h>
#inc <mach/rcw_macro.h>

#inc "kwrap/type.h"//a header for basic variable type
#inc "kwrap/task.h"
#inc <kwrap/semaphore.h>
#inc "kwrap/flag.h"
#inc <kwrap/spinlock.h>
#inc "kwrap/debug.h"
#inc <kwrap/cpu.h>



#inc <plat-na51055/nvt-sramctl.h>

#inc "kdrv_builtin/kdrv_builtin.h"
#inc "kdrv_builtin/kdrv_ipp_builtin.h"

#inc "vpe_drv.h"



#define pmc_turnonPower(a)
#define pmc_turnoffPower(a)


#define dma_getNonCacheAddr(parm) 0
#define dma_getPhyAddr(parm) 0
#define dma_flushWriteCache(parm,parm2)
#define dma_flushReadCache(parm,parm2)
#define dma_flushReadCacheWidthNEQLineOffset(parm,parm2)


#define pll_enableClock(x)
#define pll_disableClock(x)
#define pll_setClockRate(x,y)
#define pll_getPLLEn(x,y)
#define pll_setPLLEn(x)


extern UINT32 _vpe_reg_io_base;

#define _VPE_REG_BASE_ADDR      _vpe_reg_io_base
#define VPE_REG_ADDR(ofs)        (_VPE_REG_BASE_ADDR+(ofs))
#define VPE_SETREG(ofs, value)   iowrite32(value, (void *)(_VPE_REG_BASE_ADDR + ofs))
#define VPE_GETREG(ofs)          ioread32((void *)(_VPE_REG_BASE_ADDR + ofs))

#define VPE_SET_32BIT_VALUE(addr, value)    iowrite32(value, (void *)addr)



//=========================================================================
#elif defined (__FREERTOS)

#inc "string.h"
#inc <stdlib.h>
#inc "rcw_macro.h"
#inc "io_address.h"

#if defined(_BSP_NA51055_)
#inc "nvt-sramctl.h"
#endif

#inc "interrupt.h"
#inc "pll.h"
#inc "pll_protected.h"
#inc "dma_protected.h"
#inc "kwrap/type.h"
#inc <kwrap/semaphore.h>
#inc <kwrap/flag.h>
#inc <kwrap/spinlock.h>
#inc <kwrap/cpu.h>
#inc <kwrap/nvt_type.h>
#inc <kwrap/debug.h>

#inc "kdrv_builtin/kdrv_ipp_builtin.h"
//#inc "kdrv_videoprocess/kdrv_vpe.h"


//#inc "vpe_dbg.h"

#define pmc_turnonPower(a)
#define pmc_turnoffPower(a)

#define _VPE_REG_BASE_ADDR       IOADDR_VPE_REG_BASE
#define VPE_REG_ADDR(ofs)        (_VPE_REG_BASE_ADDR+(ofs))
#define VPE_SETREG(ofs, value)   OUTW((_VPE_REG_BASE_ADDR + ofs), value)
#define VPE_GETREG(ofs)          INW(_VPE_REG_BASE_ADDR + ofs)

#define VPE_SET_32BIT_VALUE(addr, value)    (*(volatile UINT32*)(addr) = (UINT32)(value))

#if (defined(_NVT_EMULATION_) == ON)
#inc "comm/timer.h"
#endif


//=========================================================================
#elif defined (__UITRON) || defined (__ECOS)

#inc <stdio.h>
#inc <stdlib.h>
#inc <string.h>

#inc "Type.h"
#inc "ErrorNo.h"
#inc "Debug.h"
#inc "dma.h"

#inc "DrvCommon.h"
#inc "interrupt.h"
//#inc "pll.h"
#inc "pll_protected.h"
#inc "top.h"
#inc "Utility.h"
#inc "Memory.h"
#inc "ist.h"


#inc "dal_ipp_utility.h"


#define __MODULE__    dal_vpe
#define __DBGLVL__    2 // 0=FATAL, 1=ERR, 2=WRN, 3=UNIT, 4=FUNC, 5=IND, 6=MSG, 7=VALUE, 8=USER
#define __DBGFLT__    "*" // *=All, [mark1]=CustomClass
#inc "DebugModule.h"


#define _VPE_REG_BASE_ADDR       IOADDR_VPE_REG_BASE
#define VPE_REG_ADDR(ofs)        (_VPE_REG_BASE_ADDR+(ofs))
#define VPE_SETREG(ofs, value)   OUTW((_VPE_REG_BASE_ADDR + ofs), value)
#define VPE_GETREG(ofs)          INW(_VPE_REG_BASE_ADDR + ofs)

#define VPE_SET_32BIT_VALUE(addr, value)    (*(volatile UINT32*)(addr) = (UINT32)(value))

//=========================================================================
#else


#endif


//-------------------------------------------------------------------------

#include "plat/top.h"

#define VPE_ALIGN_FLOOR(value, base)  ((value) & ~((base)-1))                   ///< Align Floor
#define VPE_ALIGN_ROUND(value, base)  VPE_ALIGN_FLOOR((value) + ((base)/2), base)   ///< Align Round
#define VPE_ALIGN_CEIL(value, base)   VPE_ALIGN_FLOOR((value) + ((base)-1), base)   ///< Align Ceil

#define VPE_ALIGN_ROUND_128(a)      VPE_ALIGN_ROUND(a, 128)  ///< Round Off to 128
#define VPE_ALIGN_ROUND_64(a)       VPE_ALIGN_ROUND(a, 64)  ///< Round Off to 64
#define VPE_ALIGN_ROUND_32(a)       VPE_ALIGN_ROUND(a, 32)  ///< Round Off to 32
#define VPE_ALIGN_ROUND_16(a)       VPE_ALIGN_ROUND(a, 16)  ///< Round Off to 16
#define VPE_ALIGN_ROUND_8(a)        VPE_ALIGN_ROUND(a, 8)   ///< Round Off to 8
#define VPE_ALIGN_ROUND_4(a)        VPE_ALIGN_ROUND(a, 4)   ///< Round Off to 4
#define VPE_ALIGN_ROUND_2(a)        VPE_ALIGN_ROUND(a, 2)   ///< Round Off to 2

#define VPE_ALIGN_CEIL_128(a)       VPE_ALIGN_CEIL(a, 128)  ///< Round Up to 128
#define VPE_ALIGN_CEIL_64(a)        VPE_ALIGN_CEIL(a, 64)  ///< Round Up to 64
#define VPE_ALIGN_CEIL_32(a)        VPE_ALIGN_CEIL(a, 32)   ///< Round Up to 32
#define VPE_ALIGN_CEIL_16(a)        VPE_ALIGN_CEIL(a, 16)   ///< Round Up to 16
#define VPE_ALIGN_CEIL_8(a)         VPE_ALIGN_CEIL(a, 8)    ///< Round Up to 8
#define VPE_ALIGN_CEIL_4(a)         VPE_ALIGN_CEIL(a, 4)    ///< Round Up to 4
#define VPE_ALIGN_CEIL_2(a)         VPE_ALIGN_CEIL(a, 2)    ///< Round Up to 2

#define VPE_ALIGN_FLOOR_128(a)       VPE_ALIGN_FLOOR(a, 128)  ///< Round down to 128
#define VPE_ALIGN_FLOOR_64(a)        VPE_ALIGN_FLOOR(a, 64)  ///< Round down to 64
#define VPE_ALIGN_FLOOR_32(a)       VPE_ALIGN_FLOOR(a, 32)  ///< Round down to 32
#define VPE_ALIGN_FLOOR_16(a)       VPE_ALIGN_FLOOR(a, 16)  ///< Round down to 16
#define VPE_ALIGN_FLOOR_8(a)        VPE_ALIGN_FLOOR(a, 8)   ///< Round down to 8
#define VPE_ALIGN_FLOOR_4(a)        VPE_ALIGN_FLOOR(a, 4)   ///< Round down to 4
#define VPE_ALIGN_FLOOR_2(a)        VPE_ALIGN_FLOOR(a, 2)   ///< Round down to 2


//-------------------------------------------------------------------------


#define DRV_SUPPORT_IST  0
#define _EMULATION_      0

#define FLGPTN_VPE_FRAMEEND     FLGPTN_BIT(0)
#define FLGPTN_VPE_LLERR        FLGPTN_BIT(1)
#define FLGPTN_VPE_LLEND        FLGPTN_BIT(2)
#define FLGPTN_VPE_RES0_NFDONE  FLGPTN_BIT(10)
#define FLGPTN_VPE_RES1_NFDONE  FLGPTN_BIT(11)
#define FLGPTN_VPE_RES2_NFDONE  FLGPTN_BIT(12)
#define FLGPTN_VPE_RES3_NFDONE  FLGPTN_BIT(13)




extern ER vpe_platform_flg_clear(FLGPTN flg);

extern ER vpe_platform_flg_wait(PFLGPTN p_flgptn, FLGPTN flg);

extern ER vpe_platform_flg_set(FLGPTN flg);

extern ER vpe_platform_sem_wait(VOID);

extern ER vpe_platform_sem_signal(VOID);

extern unsigned long vpe_platform_spin_lock(VOID);

extern VOID vpe_platform_spin_unlock(unsigned long loc_status);

extern VOID vpe_platform_int_enable(VOID);

extern VOID vpe_platform_int_disable(VOID);

extern VOID vpe_platform_enable_clk(VOID);

extern VOID vpe_platform_disable_clk(VOID);

extern VOID vpe_platform_prepare_clk(VOID);

extern VOID vpe_platform_unprepare_clk(VOID);

extern VOID vpe_platform_disable_sram_shutdown(VOID);

extern VOID vpe_platform_enable_sram_shutdown(VOID);

extern ER vpe_platform_set_clk_rate(UINT32 source_clk);

extern UINT32 vpe_platform_get_clk_rate(VOID);

extern UINT32 vpe_platform_va2pa(UINT32 addr);


extern UINT32 vpe_platform_dma_is_cacheable(UINT32 addr);

extern UINT32 vpe_platform_dma_flush_mem2dev_for_video_mode(UINT32 addr, UINT32 size);

extern UINT32 vpe_platform_dma_flush_mem2dev(UINT32 addr, UINT32 size);


extern UINT32 vpe_platform_dma_flush_dev2mem_width_neq_loff(UINT32 addr, UINT32 size);

extern UINT32 vpe_platform_dma_flush_dev2mem_for_video_mode(UINT32 addr, UINT32 size);

extern UINT32 vpe_platform_dma_flush_dev2mem(UINT32 addr, UINT32 size);

extern UINT32 vpe_platform_dma_post_flush_dev2mem(UINT32 addr, UINT32 size);

extern VOID vpe_platform_set_init_status(BOOL set_status);
extern BOOL vpe_platform_get_init_status(VOID);



#if !(defined __UITRON || defined __ECOS)
extern VOID vpe_isr(VOID);
#if defined (__FREERTOS)
extern VOID vpe_platform_create_resource(VOID);
#else
extern VOID vpe_platform_create_resource(VPE_INFO *pmodule_info);
#endif
extern VOID vpe_platform_release_resource(VOID);
#endif



#ifdef __cplusplus
}
#endif


#endif

