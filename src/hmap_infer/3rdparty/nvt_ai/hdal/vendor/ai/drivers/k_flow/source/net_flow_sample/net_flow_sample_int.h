/**
	@brief Header file of internal definition of vendor net flow sample.

	@file net_flow_sample_int.h

	@ingroup net_flow_sample

	@note Nothing.

	Copyright Novatek Microelectronics Corp. 2018.  All rights reserved.
*/
#ifndef _NET_FLOW_SAMPLE_INT_H_
#define _NET_FLOW_SAMPLE_INT_H_

/********************************************************************
	INCLUDE FILES
********************************************************************/
#include "net_flow_sample/net_flow_sample.h"
#include "net_flow_sample/nn_net.h"
#include "net_flow_sample/nn_diff.h"

/********************************************************************
	EXTERN VARIABLES & FUNCTION PROTOTYPES DECLARATIONS
********************************************************************/
//UINT32 vendor_ais_load_net(UINT32 addr);
ER nvt_ai_pars_net(VENDOR_AIS_FLOW_MAP_MEM_PARM *p_mem, UINT32 net_id);
//VOID vendor_ais_proc_net(INT32 in_img, UINT32 start_addr, UINT32 end_addr);
ER nvt_ai_proc_net(VENDOR_AIS_FLOW_PROC_PARM *p_parm, UINT32 net_id);
ER nvt_ai_init_mem(VENDOR_AIS_FLOW_MAP_MEM_PARM  *p_mem, UINT32 net_id);
#if !CNN_25_MATLAB
ER nvt_ai_unpars_net(VENDOR_AIS_FLOW_MAP_MEM_PARM *p_mem, UINT32 net_id);
ER nvt_ai_uninit_mem(UINT32 net_id);
ER nvt_ai_proc_input_init(UINT32 net_addr, NN_DATA *p_imem, UINT32 imem_cnt, UINT32 net_id);
#else
ER nvt_ai_uninit_mem(UINT32 net_id);
ER nvt_ai_proc_input_init(UINT32 net_addr, NN_IOMEM *p_in_io_mem, UINT32 net_id);
#endif
ER nvt_ai_proc_input_uninit(UINT32 net_addr, UINT32 net_id);
ER nvt_ai_update_layer(UINT32 layer, UINT32 net_id);
ER nvt_ai_get_net_info(NN_GEN_NET_INFO *p_info, UINT32 net_addr);
ER nvt_ai_update_net_online(VENDOR_AIS_FLOW_MAP_MEM_PARM *p_mem, NN_DIFF_MODEL_HEAD *p_model_head, UINT32 model_id, UINT32 net_id);
ER nvt_ai_restore_net_online(VENDOR_AIS_FLOW_MAP_MEM_PARM *p_mem, NN_DIFF_MODEL_HEAD *p_model_head, UINT32 model_id, UINT32 net_id);
ER nvt_ai_proc_input_layer_init(UINT32 net_addr, NN_DATA *p_imem, UINT32 imem_cnt, UINT32 proc_idx, UINT32 net_id);
ER nvt_ai_proc_input_layer_uninit(UINT32 net_addr, UINT32 proc_idx, UINT32 net_id);
#if !CNN_25_MATLAB
ER nvt_ai_proc_input_layer_init2(UINT32 net_addr, NN_DATA *p_imem, UINT32 imem_cnt, UINT32 proc_idx, UINT32 net_id);
ER nvt_ai_proc_input_layer_uninit2(UINT32 net_addr, UINT32 proc_idx, UINT32 net_id);
#endif
VOID nvt_ai_reset_status(VOID);
ER nvt_ai_update_net_online_batch(VENDOR_AIS_FLOW_MAP_MEM_PARM *p_mem, NN_DIFF_MODEL_HEAD *p_model_head, UINT32 model_id, UINT32 net_id);
ER nvt_ai_restore_net_online_batch(VENDOR_AIS_FLOW_MAP_MEM_PARM *p_mem, NN_DIFF_MODEL_HEAD *p_model_head, UINT32 model_id, UINT32 net_id);
#endif  /* _NET_FLOW_SAMPLE_INT_H_ */
