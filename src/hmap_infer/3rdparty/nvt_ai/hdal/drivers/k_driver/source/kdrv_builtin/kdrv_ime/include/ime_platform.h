/*
    IME module driver

    NT98520 IME internal header file.

    @file       ime_platform.h
    @ingroup    mIIPPIME
    @note       Nothing

    Copyright   Novatek Microelectronics Corp. 2019.  All rights reserved.
*/

#ifndef _IME_PLATFORM_H_
#define _IME_PLATFORM_H_

#ifdef __cplusplus
extern "C" {
#endif


#define IME_ALIGN_FLOOR(value, base)  ((value) & ~((base)-1))                   ///< Align Floor
#define IME_ALIGN_ROUND(value, base)  IME_ALIGN_FLOOR((value) + ((base)/2), base)   ///< Align Round
#define IME_ALIGN_CEIL(value, base)   IME_ALIGN_FLOOR((value) + ((base)-1), base)   ///< Align Ceil

#define IME_ALIGN_ROUND_64(a)       IME_ALIGN_ROUND(a, 64)  ///< Round Off to 64
#define IME_ALIGN_ROUND_32(a)       IME_ALIGN_ROUND(a, 32)  ///< Round Off to 32
#define IME_ALIGN_ROUND_16(a)       IME_ALIGN_ROUND(a, 16)  ///< Round Off to 16
#define IME_ALIGN_ROUND_8(a)        IME_ALIGN_ROUND(a, 8)   ///< Round Off to 8
#define IME_ALIGN_ROUND_4(a)        IME_ALIGN_ROUND(a, 4)   ///< Round Off to 4

#define IME_ALIGN_CEIL_64(a)        IME_ALIGN_CEIL(a, 64)   ///< Round Up to 64
#define IME_ALIGN_CEIL_32(a)        IME_ALIGN_CEIL(a, 32)   ///< Round Up to 32
#define IME_ALIGN_CEIL_16(a)        IME_ALIGN_CEIL(a, 16)   ///< Round Up to 16
#define IME_ALIGN_CEIL_8(a)         IME_ALIGN_CEIL(a, 8)    ///< Round Up to 8
#define IME_ALIGN_CEIL_4(a)         IME_ALIGN_CEIL(a, 4)    ///< Round Up to 4

#define IME_ALIGN_FLOOR_64(a)       IME_ALIGN_FLOOR(a, 64)  ///< Round down to 64
#define IME_ALIGN_FLOOR_32(a)       IME_ALIGN_FLOOR(a, 32)  ///< Round down to 32
#define IME_ALIGN_FLOOR_16(a)       IME_ALIGN_FLOOR(a, 16)  ///< Round down to 16
#define IME_ALIGN_FLOOR_8(a)        IME_ALIGN_FLOOR(a, 8)   ///< Round down to 8
#define IME_ALIGN_FLOOR_4(a)        IME_ALIGN_FLOOR(a, 4)   ///< Round down to 4

//-------------------------------------------------------------------------
#include "plat/top.h"


#if defined (__KERNEL__)

#inc <linux/io.h>
#inc <linux/spinlock.h>
#inc <linux/semaphore.h>
#inc <linux/interrupt.h>
#inc <linux/completion.h>
#inc <linux/clk.h>
#inc <linux/module.h>
#inc <linux/export.h>
#inc <linux/dma-mapping.h> // header file Dma(cache handle)

#inc "rcw_macro.h"
#inc "mach/nvt-io.h"
#inc <mach/fmem.h>

#inc "kwrap/type.h"//a header for basic variable type
#inc "kwrap/task.h"
#inc "kwrap/semaphore.h"
#inc "kwrap/flag.h"
#inc "kwrap/spinlock.h"
#inc "kwrap/debug.h"
#inc <kwrap/cpu.h>



#inc <plat-na51055/nvt-sramctl.h>

#inc "kdrv_builtin/kdrv_builtin.h"
#inc "kdrv_builtin/kdrv_ipp_builtin.h"

//#inc "ime_dbg.h"
#inc "ime_drv.h"

//#inc "ime_int.h"
//#inc "ime_lib.h"


//#inc "kdrv_type.h"
//#inc "kdrv_videoprocess/kdrv_ipp.h"
//#inc "kdrv_videoprocess/kdrv_ipp_config.h"
//#inc "kdrv_videoprocess/kdrv_ipp_utility.h"
//#inc "kdrv_videoprocess/kdrv_ime.h"
//#inc "kdrv_ime_int.h"

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


extern UINT32 _ime_reg_io_base;

#define _IME_REG_BASE_ADDR      _ime_reg_io_base
#define IME_REG_ADDR(ofs)        (_IME_REG_BASE_ADDR+(ofs))
#define IME_SETREG(ofs, value)   kdrv_ipp_set_reg(KDRV_IPP_BUILTIN_IME, _IME_REG_BASE_ADDR, ofs, value)
#define IME_GETREG(ofs)          ioread32((void *)(_IME_REG_BASE_ADDR + ofs))

#define IME_SET_32BIT_VALUE(addr, value)    iowrite32(value, (void *)addr)

extern struct clk *ime_clk[IME_CLK_NUM];
extern struct clk *ime_pclk[IME_CLK_NUM];


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

//#inc "ime_dbg.h"

#define pmc_turnonPower(a)
#define pmc_turnoffPower(a)

#define _IME_REG_BASE_ADDR       IOADDR_IME_REG_BASE  //(0xF0C40000)
#define IME_REG_ADDR(ofs)        (_IME_REG_BASE_ADDR+(ofs))
#define IME_SETREG(ofs, value)   OUTW((_IME_REG_BASE_ADDR + ofs), value)
#define IME_GETREG(ofs)          INW(_IME_REG_BASE_ADDR + ofs)

#define IME_SET_32BIT_VALUE(addr, value)    (*(volatile UINT32*)(addr) = (UINT32)(value))

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


#define __MODULE__    dal_ime
#define __DBGLVL__    2 // 0=FATAL, 1=ERR, 2=WRN, 3=UNIT, 4=FUNC, 5=IND, 6=MSG, 7=VALUE, 8=USER
#define __DBGFLT__    "*" // *=All, [mark1]=CustomClass
#inc "DebugModule.h"


#define _IME_REG_BASE_ADDR       IOADDR_IME_REG_BASE  //(0xF0C40000)
#define IME_REG_ADDR(ofs)        (_IME_REG_BASE_ADDR+(ofs))
#define IME_SETREG(ofs, value)   OUTW((_IME_REG_BASE_ADDR + ofs), value)
#define IME_GETREG(ofs)          INW(_IME_REG_BASE_ADDR + ofs)

#define IME_SET_32BIT_VALUE(addr, value)    (*(volatile UINT32*)(addr) = (UINT32)(value))

//=========================================================================
#else


#endif


//-------------------------------------------------------------------------


#define DRV_SUPPORT_IST  0
#define _EMULATION_      0

#define FLGPTN_IME_STRIPE_END   FLGPTN_BIT(0)
#define FLGPTN_IME_FRAMEEND     FLGPTN_BIT(1)
//#define FLGPTN_IME_DONE         FLGPTN_BIT(2)
#define FLGPTN_IME_LLEND        FLGPTN_BIT(2)
#define FLGPTN_IME_JEND         FLGPTN_BIT(3)
#define FLGPTN_IME_BP1          FLGPTN_BIT(4)
#define FLGPTN_IME_BP2          FLGPTN_BIT(5)
#define FLGPTN_IME_BP3          FLGPTN_BIT(6)
#define FLGPTN_IME_FRAMESTART   FLGPTN_BIT(7)


extern BOOL ime_ctrl_flow_to_do;
extern BOOL fw_ime_power_saving_en;

extern ER ime_platform_flg_clear(FLGPTN flg);

extern ER ime_platform_flg_wait(PFLGPTN p_flgptn, FLGPTN flg);

extern ER ime_platform_flg_set(FLGPTN flg);

extern ER ime_platform_sem_wait(VOID);

extern ER ime_platform_sem_signal(VOID);

extern unsigned long ime_platform_spin_lock(VOID);

extern VOID ime_platform_spin_unlock(unsigned long loc_status);

extern VOID ime_platform_int_enable(VOID);

extern VOID ime_platform_int_disable(VOID);

extern VOID ime_platform_enable_clk(VOID);

extern VOID ime_platform_disable_clk(VOID);

extern VOID ime_platform_prepare_clk(VOID);

extern VOID ime_platform_unprepare_clk(VOID);

extern VOID ime_platform_disable_sram_shutdown(VOID);

extern VOID ime_platform_enable_sram_shutdown(VOID);

extern ER ime_platform_set_clk_rate(UINT32 source_clk);

extern UINT32 ime_platform_get_clk_rate(VOID);

extern UINT32 ime_platform_va2pa(UINT32 addr);


extern UINT32 ime_platform_dma_is_cacheable(UINT32 addr);

extern UINT32 ime_platform_dma_flush_mem2dev_for_video_mode(UINT32 addr, UINT32 size);

extern UINT32 ime_platform_dma_flush_mem2dev(UINT32 addr, UINT32 size);


extern UINT32 ime_platform_dma_flush_dev2mem_width_neq_loff(UINT32 addr, UINT32 size);

extern UINT32 ime_platform_dma_flush_dev2mem_for_video_mode(UINT32 addr, UINT32 size);

extern UINT32 ime_platform_dma_flush_dev2mem(UINT32 addr, UINT32 size);

extern UINT32 ime_platform_dma_post_flush_dev2mem(UINT32 addr, UINT32 size);

extern VOID ime_platform_set_init_status(BOOL set_status);
extern BOOL ime_platform_get_init_status(VOID);



#if !(defined __UITRON || defined __ECOS)
extern VOID ime_isr(VOID);
#if defined __FREERTOS
extern VOID ime_platform_create_resource(VOID);
#else
extern VOID ime_platform_create_resource(IME_INFO *pmodule_info);
#endif
extern VOID ime_platform_release_resource(VOID);
#endif


#ifdef __cplusplus
}
#endif


#endif // _IME_PM_REG_

