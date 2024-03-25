/**
	@brief Header file of debug definition of vendor net flow sample.

	@file net_flow_sample.h

	@ingroup net_flow_sample

	@note Nothing.

	Copyright Novatek Microelectronics Corp. 2018.  All rights reserved.
*/
#ifndef _NET_FLOW_SAMPLE_DBG_H_
#define _NET_FLOW_SAMPLE_DBG_H_

/********************************************************************
	INCLUDE FILES
********************************************************************/
//#inc <linux/kernel.h>
#include "kwrap/debug.h"
/********************************************************************
	MACRO CONSTANT DEFINITIONS
********************************************************************/
#if 0
#define NVT_DBG_INFO                    0x00000001
#define NVT_DBG_WARN                    0x00000002
#define NVT_DBG_ERR                     0x00000004
#define DEBUG                           1

/*#define INFO                    0x00000001
#define WARN                    0x00000002
#define ERR                     0x00000004*/

#ifdef DEBUG
extern unsigned int xxx_debug_level;

#define nvt_dbg(level, fmt, args...)            \
	do {                                        \
		printk("%s:" fmt, __func__, ##args);    \
	} while (0)
#else
#define nvt_dbg(fmt, ...)
#endif
#endif
#endif  /* _NET_FLOW_SAMPLE_DBG_H_ */
