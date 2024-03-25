/**
	@brief Header file of definition of vendor net layer sample.

	@file net_layer_sample.h

	@ingroup net_layer_sample

	@note Nothing.

	Copyright Novatek Microelectronics Corp. 2019.  All rights reserved.
*/
#ifndef _NET_LAYER_SAMPLE_H_
#define _NET_LAYER_SAMPLE_H_

/********************************************************************
	INCLUDE FILES
********************************************************************/
#include "hd_type.h"
#include "net_flow_sample/nn_net.h"
#include "custnn/net_cust_layer.h"

/********************************************************************
	MACRO CONSTANT DEFINITIONS
********************************************************************/
#define NN_LAYER_LIB_CUSTNN     0x00000000
#define NN_LAYER_LIB_NVTNN      0x80000000
#define NN_LAYER_TYPE_ID_MASK   0x7fffffff

/********************************************************************
	EXTERN VARIABLES & FUNCTION PROTOTYPES DECLARATIONS
********************************************************************/
#if USE_NEON
extern HD_RESULT vendor_ai_nvtnn(NN_GEN_ENG_TYPE eng, UINT32 parm_addr, UINT32 net_id);
#endif

#endif  /* _NET_LAYER_SAMPLE_H_ */
