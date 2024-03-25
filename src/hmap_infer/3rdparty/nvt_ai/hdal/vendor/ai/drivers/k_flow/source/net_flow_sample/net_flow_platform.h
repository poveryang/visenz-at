/**
	@brief Header file of internal definition of vendor net flow sample.

	@file net_flow_platform.h

	@ingroup net_flow_platform

	@note Nothing.

	Copyright Novatek Microelectronics Corp. 2018.  All rights reserved.
*/
#ifndef _NET_FLOW_PLATFORM_H_
#define _NET_FLOW_PLATFORM_H_

/********************************************************************
	INCLUDE FILES
********************************************************************/

/********************************************************************
	EXTERN VARIABLES & FUNCTION PROTOTYPES DECLARATIONS
********************************************************************/
UINT32 nvt_ai_pa2va_remap(UINT32 pa, UINT32 sz);
UINT32 nvt_ai_pa2va_remap_wo_sync(UINT32 pa, UINT32 sz);
VOID nvt_ai_pa2va_unmap(UINT32 va, UINT32 pa);
UINT32 nvt_ai_chk_ic_ver(VOID);

#endif  /* _NET_FLOW_PLATFORM_H_ */
