/**
	@brief Header file of definition of vendor net generation sample.

	@file net_gen_sample.h

	@ingroup net_gen_sample

	@note Nothing.

	Copyright Novatek Microelectronics Corp. 2018.  All rights reserved.
*/
#ifndef _NET_GEN_SAMPLE_H_
#define _NET_GEN_SAMPLE_H_

/********************************************************************
	INCLUDE FILES
********************************************************************/
#include "hd_type.h"
#include "kwrap/type.h"
#include "vendor_ai/vendor_ai.h"
#include "net_flow_sample/net_flow_sample.h"
#include "net_pre_sample/net_pre_sample.h"
#include "net_flow_sample/nn_diff.h"

/********************************************************************
 MACRO CONSTANT DEFINITIONS
********************************************************************/
#define NN_SUPPORT_NET_MAX		128
#define MODIFY_ENG_ONLINE       1

/********************************************************************
	TYPE DEFINITION
********************************************************************/

/********************************************************************
	EXTERN VARIABLES & FUNCTION PROTOTYPES DECLARATIONS
********************************************************************/
extern HD_RESULT vendor_ais_net_gen_init(VENDOR_AIS_FLOW_MAP_MEM_PARM map_mem, UINT32 net_id);
extern HD_RESULT vendor_ais_net_gen_uninit(UINT32 net_id);
extern UINT32 vendor_ais_user_parm_va2pa(UINT32 addr, UINT32 net_id);
extern UINT32 vendor_ais_user_parm_pa2va(UINT32 addr, UINT32 net_id);
extern UINT32 vendor_ais_user_model_va2pa(UINT32 addr, UINT32 net_id);
extern UINT32 vendor_ais_user_model_pa2va(UINT32 addr, UINT32 net_id);
extern UINT32 vendor_ais_user_buff_va2pa(UINT32 addr, UINT32 net_id);
extern UINT32 vendor_ais_user_buff_pa2va(UINT32 addr, UINT32 net_id);
extern ER vendor_ais_cal_size(UINT32 s_addr, UINT32 *p_parm_sz, UINT32 *p_model_sz, UINT32 *p_buf_sz);
extern UINT32 vendor_ais_auto_alloc_mem(VENDOR_AIS_FLOW_MEM_PARM *p_mem, VENDOR_AIS_FLOW_MAP_MEM_PARM *p_mem_manager);
extern UINT32 vendor_ais_get_max_buf_sz_layer(VENDOR_AIS_FLOW_MEM_PARM *p_mem);
extern HD_RESULT vendor_ais_net_input_init(VENDOR_AIS_IMG_PARM *p_input_info, UINT32 net_id);
extern HD_RESULT vendor_ais_net_input_uninit(UINT32 net_id);
extern HD_RESULT vendor_ais_net_engine_open(VENDOR_AI_ENG engine, UINT32 net_id);
extern HD_RESULT vendor_ais_net_engine_close(VENDOR_AI_ENG engine, UINT32 net_id);
#if MODIFY_ENG_ONLINE
extern HD_RESULT vendor_ais_pars_engid(UINT32 eng_number, UINT32 net_id);
extern HD_RESULT vendor_ais_pars_engid_perlayer(UINT32 eng_number_addr, UINT32 net_id);
#endif
extern HD_RESULT vendor_ais_get_model_mem_sz(UINT32 *require_sz, CHAR *p_filename);
extern CHAR* vendor_ais_chk_sdk_version(VOID);
extern UINT32 vendor_ais_load_file(CHAR *p_filename, UINT32 va, NN_LAYER_OUTPUT_INFO** output_layer_info, UINT32 *num_output_layer);
extern HD_RESULT vendor_ais_find_layer_info_by_name(char *layer_name, NN_LAYER_OUTPUT_INFO *output_layer_info, NN_LAYER_OUTPUT_INFO **dst_layer_info, UINT32 num_output_layer);
extern HD_RESULT vendor_ais_get_layer_mem(VENDOR_AIS_FLOW_MEM_PARM mem_parm, NN_IOMEM *p_layer_mem, UINT32 layer_num);
extern HD_RESULT vendor_ais_transform_float_output(NN_LAYER_OUTPUT_INFO *layer_info, VOID *layer_out_mem, float *layer_out_tran_float);
extern HD_RESULT vendor_ais_get_diff_model_mem_sz(UINT32 *require_sz, CHAR *p_filename);

extern HD_RESULT vendor_ais_get_model_mem_sz_from_mem(UINT32 addr, UINT32 addr_len, UINT32 *require_sz);
extern UINT32 vendor_ais_load_from_hdal_mem(UINT32 va, UINT32 va_len, NN_LAYER_OUTPUT_INFO** output_layer_info, UINT32 *num_output_layer);
extern UINT32 vendor_ais_get_net_int_status(UINT32 net_id);
#if !CNN_25_MATLAB
extern HD_RESULT vendor_ais_get_net_input_info(NN_IN_OUT_FMT **input_info, VENDOR_AIS_FLOW_MEM_PARM net_mem);
#endif
extern HD_RESULT vendor_ais_net_input_layer_init(VENDOR_AIS_IMG_PARM *p_input_info, UINT32 proc_idx, UINT32 net_id);
extern HD_RESULT vendor_ais_net_input_layer_uninit(UINT32 proc_idx, UINT32 net_id);
extern UINT32 vendor_ais_net_get_input_layer_index(VENDOR_AIS_FLOW_MEM_PARM mem, UINT32* p_input_idx_set);
extern UINT32 vendor_ais_get_net_is_batch(UINT32 net_id);
extern UINT32 vendor_ais_net_get_input_blob_info(VENDOR_AIS_FLOW_MEM_PARM mem, UINT32 input_blob_idx, UINT32 *input_blob_io_num, UINT32** p_input_blob_info);
extern HD_RESULT vendor_ais_get_layer_mem_v2(VENDOR_AIS_FLOW_MEM_PARM mem_parm, NN_DATA *p_layer_mem, NN_LAYER_OUTPUT_INFO *output_layer_info);
extern HD_RESULT vendor_ais_net_input_init_v2(VENDOR_AIS_IMG_PARM_V2 *p_input_info, UINT32 net_id);
extern HD_RESULT vendor_ais_get_input_layer_mem(VENDOR_AIS_FLOW_MEM_PARM mem_parm, NN_DATA *p_layer_mem, UINT32 input_proc_idx);
extern HD_RESULT vendor_ais_float_to_fix(UINT32 in_addr, UINT32 out_addr, NN_FMT bit_info, UINT32 size, FLOAT scale_ratio);
extern UINT32 vendor_ais_load_file_wo_set_out_info(CHAR *p_filename, UINT32 va);
extern UINT32 vendor_ais_get_output_info_size(UINT32 va);
extern UINT32 vendor_ais_get_input_info_size(UINT32 va);
extern UINT32 vendor_ais_get_output_layer_info(UINT32 model_va, NN_LAYER_OUTPUT_INFO* output_layer_info, UINT32* num_output_layer);
#endif  /* _NET_GEN_SAMPLE_H_ */
