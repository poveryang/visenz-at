/**
	@brief Header file of definition of vendor user-space net flow sample.

	@file net_flow_user_sample.h

	@ingroup net_flow_user_sample

	@note Nothing.

	Copyright Novatek Microelectronics Corp. 2018.  All rights reserved.
*/
#ifndef _NET_FLOW_USER_SAMPLE_H_
#define _NET_FLOW_USER_SAMPLE_H_

#define NET_FLOW_USR_SAMPLE_LAYER_OUT	TRUE

/********************************************************************
	INCLUDE FILES
********************************************************************/
#include "hd_type.h"
#include "net_flow_sample/net_flow_sample.h"
#include "net_pre_sample/net_pre_sample.h"
#include "net_flow_sample/nn_parm.h"
#include "ai_ioctl.h"

/********************************************************************
	MACRO CONSTANT DEFINITIONS
********************************************************************/
#define VENDOR_AIS_LBL_LEN      256     ///< maximal length of class label
#define MAX_BATCH_NUM           10
#define MAX_CLASS_NUM           1000
#define TOP_N                   5
#define DYNAMIC_LABEL 			1
/********************************************************************
	TYPE DEFINITION
********************************************************************/
/**
    Result information.
*/
typedef struct _VENDOR_AIS_RESULT {
	INT32 no[TOP_N];						///< class number
	FLOAT score[TOP_N];						///< class score
#if USE_NEON	
	FLOAT x;
    FLOAT y;
	FLOAT w;
    FLOAT h;
#endif
} VENDOR_AIS_RESULT;

typedef struct _VENDOR_AIS_RESULT_INFO {
	UINT32 result_num;
	VENDOR_AIS_RESULT *p_result;
} VENDOR_AIS_RESULT_INFO;

/*
    AI dbg mode
*/
typedef enum {
	AI_DUMP_INFO     = 0,
	AI_DUMP_OUT_BIN  = 1,
	AI_DUMP_IN_BIN   = 2,
	AI_DBG_STOP_IDX  = 3,
	AI_DUMP_LAST_OUT_BIN = 4,
	ENUM_DUMMY4WORD(VENDOR_AIS_DBG_MODE)
} VENDOR_AIS_DBG_MODE;
/********************************************************************
	EXTERN VARIABLES & FUNCTION PROTOTYPES DECLARATIONS
********************************************************************/
extern HD_RESULT vendor_ais_init(UINT32 net_id);
extern HD_RESULT vendor_ais_uninit(UINT32 net_id);
extern HD_RESULT vendor_ais_remap_kerl_mem(VENDOR_AIS_FLOW_MAP_MEM_INFO *p_info);
extern HD_RESULT vendor_ais_unmap_kerl_mem(VENDOR_AIS_FLOW_MAP_MEM_INFO *p_info);
extern HD_RESULT vendor_ais_pars_kerl_mem(VENDOR_AIS_FLOW_MAP_MEM_INFO *p_info);
extern INT32 vendor_ais_flow_user_init(VENDOR_AIS_FLOW_MAP_MEM_PARM mem, UINT32 net_id);
extern INT32 vendor_ais_flow_user_uninit(UINT32 net_id);
extern HD_RESULT vendor_ais_set_ll_info(AI_DRV_LL_USR_INFO *ll_usr_info, UINT32 net_id);
#if USE_NEON
extern HD_RESULT vendor_ais_proc_net(VENDOR_AIS_FLOW_MEM_PARM net_mem, VENDOR_AIS_FLOW_MEM_PARM rslt_mem, VENDOR_AIS_IMG_PARM *p_src_img, UINT32 net_id);
#else
extern HD_RESULT vendor_ais_proc_net(VENDOR_AIS_FLOW_MEM_PARM net_mem, VENDOR_AIS_FLOW_MEM_PARM rslt_mem, UINT32 net_id);
#endif
#if CNN_25_MATLAB
extern HD_RESULT vendor_ais_proc_input_init(NN_MODE mode, NN_IOMEM *p_1st_mem, VENDOR_AIS_FLOW_MEM_PARM mem, UINT32 net_id);
extern HD_RESULT vendor_ais_proc_input_uninit(NN_IOMEM *p_1st_mem, VENDOR_AIS_FLOW_MEM_PARM mem, UINT32 net_id);
#else
extern HD_RESULT vendor_ais_unpars_kerl_mem(VENDOR_AIS_FLOW_MAP_MEM_INFO *p_info);
extern HD_RESULT vendor_ais_proc_input_init(NN_MODE mode, NN_DATA *p_1st_imem, VENDOR_AIS_FLOW_MEM_PARM mem, UINT32 net_id);
extern HD_RESULT vendor_ais_proc_input_uninit(NN_DATA *p_1st_imem, VENDOR_AIS_FLOW_MEM_PARM mem, UINT32 net_id);
#endif
extern NN_GEN_MODE_CTRL *vendor_ais_get_proclayer(UINT32 layer, VENDOR_AIS_FLOW_MEM_PARM mem);
extern HD_RESULT vendor_ais_update_proclayer(UINT32 layer, UINT32 net_id);
extern VENDOR_AIS_RESULT_INFO *vendor_ais_get_results(UINT32 net_id);
extern HD_RESULT vendor_ais_get_net_info(NN_GEN_NET_INFO *p_info, UINT32 net_addr);
extern HD_RESULT vendor_ais_pars_diff_mem(VENDOR_AIS_FLOW_MAP_MEM_PARM *p_mem, VENDOR_AIS_FLOW_MEM_PARM* p_diff_mem, VENDOR_AIS_DIFF_MODEL_INFO *p_diff_info, UINT32 net_id);
extern HD_RESULT vendor_ais_unpars_diff_mem(VENDOR_AIS_FLOW_MAP_MEM_PARM *p_mem, VENDOR_AIS_FLOW_MEM_PARM* p_diff_mem, VENDOR_AIS_DIFF_MODEL_INFO *p_diff_info, UINT32 net_id);
#if DYNAMIC_LABEL
extern HD_RESULT vendor_ais_get_label_num(UINT32 *label_num, VENDOR_AIS_FLOW_MEM_PARM net_mem, UINT32 net_id);
#endif
extern HD_RESULT vendor_ais_set_dbg_mode(VENDOR_AIS_DBG_MODE dbg_info, UINT32 val);
extern HD_RESULT vendor_ais_get_net_dim(VENDOR_AIS_IMG_PARM *input_dim, VENDOR_AIS_FLOW_MEM_PARM net_mem);
#if CNN_25_MATLAB
extern HD_RESULT vendor_ais_proc_input_layer_init(NN_MODE mode, NN_IOMEM *p_io_mem, VENDOR_AIS_FLOW_MEM_PARM mem, UINT32 proc_idx, UINT32 net_id);
#else
extern HD_RESULT vendor_ais_proc_input_layer_init(NN_MODE mode, NN_DATA *p_io_mem, VENDOR_AIS_FLOW_MEM_PARM mem, UINT32 proc_idx, UINT32 net_id);
#endif
extern HD_RESULT vendor_ais_proc_input_layer_uninit(UINT32 proc_idx, VENDOR_AIS_FLOW_MEM_PARM mem, UINT32 net_id);
#if !CNN_25_MATLAB
HD_RESULT vendor_ais_proc_input_layer_init2(NN_MODE mode, NN_DATA *p_1st_imem, VENDOR_AIS_FLOW_MEM_PARM mem, UINT32 proc_idx, UINT32 net_id);
HD_RESULT vendor_ais_proc_input_layer_uninit2(NN_DATA *p_1st_imem, VENDOR_AIS_FLOW_MEM_PARM mem, UINT32 proc_idx, UINT32 net_id);
#endif
extern VOID vendor_ai_global_init(VOID);
extern VOID vendor_ai_global_uninit(VOID);
extern INT32 vendor_ai_get_kflow_version(CHAR* kflow_version);
extern HD_RESULT vendor_ais_proc_net_v2(VENDOR_AIS_FLOW_MEM_PARM net_mem, VENDOR_AIS_FLOW_MEM_PARM rslt_mem, VENDOR_AIS_IMG_PARM *p_src_img, UINT32 net_id, UINT32 cpu_id);

extern HD_RESULT vendor_ais_pars_diff_batch_mem(VENDOR_AIS_FLOW_MAP_MEM_PARM *p_mem,  VENDOR_AIS_DIFF_BATCH_MODEL_INFO *p_diff_info, UINT32 net_id);
extern HD_RESULT vendor_ais_unpars_diff_batch_mem(VENDOR_AIS_FLOW_MAP_MEM_PARM *p_mem, VENDOR_AIS_DIFF_BATCH_MODEL_INFO *p_diff_info, UINT32 net_id);
extern HD_RESULT vendor_ais_get_diff_batch_id_num(VENDOR_AIS_FLOW_MAP_MEM_PARM *p_mem, UINT32 *batch_id_num);
extern HD_RESULT vendor_ais_get_diff_batch_id_info(VENDOR_AIS_FLOW_MAP_MEM_PARM *p_mem, UINT32 *max_batch_num, UINT32* p_batch_id);
extern HD_RESULT vendor_ais_set_usr_info(VOID* p_usr_info, UINT32 net_id);
extern HD_RESULT vendor_ais_get_net_id(UINT32* net_id);
extern HD_RESULT vendor_ais_release_net_id(UINT32 net_id);
#endif  /* _NET_FLOW_USER_SAMPLE_H_ */
