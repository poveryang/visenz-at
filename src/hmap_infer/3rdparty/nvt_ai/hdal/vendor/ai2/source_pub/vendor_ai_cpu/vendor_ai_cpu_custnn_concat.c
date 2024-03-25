/**
	@brief Source file of custom layers.

	@file net_cust_layer.c

	@ingroup custnn

	@note Nothing.

	Copyright Novatek Microelectronics Corp. 2019.  All rights reserved.
*/

/*-----------------------------------------------------------------------------*/
/* Include Files                                                               */
/*-----------------------------------------------------------------------------*/
#include "hd_type.h"
#include "vendor_ai_util.h"
#include "vendor_ai_cpu_custnn.h"         // NN_CUSTOMER
#include "vendor_ai_cpu_custnn_sample.h"
#include "hd_common.h"
#include <string.h>

/*-----------------------------------------------------------------------------*/
/* Macro Constant Definitions                                                  */
/*-----------------------------------------------------------------------------*/
#define CUST_CONCAT_PRINT_PARM FALSE
#define BITOFBYTE  8
#define DIMNUM     5

/*-----------------------------------------------------------------------------*/
/* Interface Functions                                                         */
/*-----------------------------------------------------------------------------*/

UINT32 custnn_cpu_fetch_input_output_size(NN_CUSTOM_DIM* tensor, NN_FMT* fmt) {
	UINT32 mul = 1;
	for(UINT32 i = 0; i < DIMNUM; i++) {
		if(tensor->dim[i]!=0) {
			mul = mul * tensor->dim[i];
		}
	}
	UINT32 bitdepth = (fmt->frac_bits + fmt->int_bits + fmt->sign_bits) / BITOFBYTE;
	return mul * bitdepth;
}

#if CUST_SUPPORT_MULTI_IO
HD_RESULT custnn_cpu_concat_process(CUSTNN_CONCAT_PARM* p_parm, UINTPTR temp_buf_addr, 
 NN_DATA* input_info, NN_CUSTOM_DIM* input_dim, INT32 *p_isf_parm, UINT32 input_num, 
 NN_DATA* output_info, NN_CUSTOM_DIM* output_dim, INT32 *p_osf_parm, UINT32 output_num)
{
	UINT32 i = 0, datasize;
	UINTPTR output_addr = 0;
	INT32 shift = 0;
	UINTPTR *proc_in_addr = NULL;
	UINTPTR temp_buf_pointer = temp_buf_addr;
	
	#if CUST_CONCAT_PRINT_PARM
	printf("in1 addr = 0x%08X\n", (unsigned int)input_info[0].va);
	#endif
	
	proc_in_addr = (UINTPTR*)malloc(input_num*sizeof(UINTPTR));
	for (i = 0; i < input_num; i++) {
		proc_in_addr[i] = temp_buf_pointer;
		datasize = custnn_cpu_fetch_input_output_size(&input_dim[i], &input_info[i].fmt);
		if (proc_in_addr[i] == 0 || input_info[i].va == 0) {
			printf("concat fail: null input\n");
			free(proc_in_addr);
			return -1;
		}
		custnn_cpu_scaleshift_process(input_info[i].va, proc_in_addr[i], datasize, custnn_get_type(input_info[i].fmt), p_isf_parm[2*i], p_isf_parm[2*i+1]);
		temp_buf_pointer += datasize;
	}
	
	output_addr = output_info[0].va;
	if (output_addr == 0) {
		printf("concat fail: null output\n");
		free(proc_in_addr);
		return -1;
	}
	
	for (i = 0; i < input_num; i++) {
		datasize = custnn_cpu_fetch_input_output_size(&input_dim[i], &input_info[i].fmt);
		memcpy((VOID*)output_addr, (VOID*)proc_in_addr[i], datasize);
		if (input_info[i].fmt.frac_bits != output_info[0].fmt.frac_bits) {
			shift = input_info[i].fmt.frac_bits - output_info[0].fmt.frac_bits;
			custnn_cpu_scaleshift_process(output_addr, output_addr, datasize, custnn_get_type(input_info[i].fmt), 1, shift);
		}
		output_addr += datasize;
	}
	
	for (i = 0; i < output_num; i++) {
		datasize = custnn_cpu_fetch_input_output_size(&output_dim[i], &output_info[i].fmt);
		custnn_cpu_scaleshift_process(output_info[i].va, output_info[i].va, datasize, custnn_get_type(output_info[i].fmt), p_osf_parm[2*i], p_osf_parm[2*i+1]);
	}
	
	free(proc_in_addr);
	
	return HD_OK;
}
#endif