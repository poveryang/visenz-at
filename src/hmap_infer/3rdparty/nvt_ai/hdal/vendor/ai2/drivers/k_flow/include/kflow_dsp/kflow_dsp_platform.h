/**
	@brief Source file of kflow_dsp.

	@file kflow_dsp_platform.h

	@ingroup kflow_dsp
	
	@note Nothing.

	Copyright Novatek Microelectronics Corp. 2018.  All rights reserved.
*/

/*-----------------------------------------------------------------------------*/
/* Include Files                                                               */
/*-----------------------------------------------------------------------------*/
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

extern void* nvt_ai_mem_alloc_dsp(unsigned size);

extern void nvt_ai_mem_free_dsp(void* addr);
