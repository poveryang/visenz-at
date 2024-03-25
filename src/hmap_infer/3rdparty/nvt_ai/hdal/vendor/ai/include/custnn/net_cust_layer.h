/**
	@brief Header file of definition of network custom layer.

	@file net_cust_layer.h

	@ingroup custnn

	@note Nothing.

	Copyright Novatek Microelectronics Corp. 2019.  All rights reserved.
*/
#ifndef _NET_CUST_LAYER_H_
#define _NET_CUST_LAYER_H_

/********************************************************************
	INCLUDE FILES
********************************************************************/
#include "hd_type.h"
#include "net_flow_sample/nn_net.h"

/********************************************************************
	MACRO CONSTANT DEFINITIONS
********************************************************************/
#define NN_USE_DSP              FALSE

/********************************************************************
	EXTERN VARIABLES & FUNCTION PROTOTYPES DECLARATIONS
********************************************************************/
extern HD_RESULT vendor_ai_cust_init  (NN_GEN_ENG_TYPE eng, UINT32 parm_addr, UINT32 net_id);
extern HD_RESULT vendor_ai_cust_uninit(NN_GEN_ENG_TYPE eng, UINT32 parm_addr, UINT32 net_id);
extern HD_RESULT vendor_ai_cust(NN_GEN_ENG_TYPE eng, UINT32 parm_addr, UINT32 net_id, VOID* usr_info);
extern HD_RESULT vendor_ai_cust_set_tmp_buf(NN_DATA tmp_buf, UINT32 parm_addr);
extern UINT32 vendor_ai_cust_get_layer_id(UINT32 parm_addr);
extern const CHAR *vendor_ai_cust_get_version(VOID);
#endif  /* _NET_CUST_LAYER_H_ */
