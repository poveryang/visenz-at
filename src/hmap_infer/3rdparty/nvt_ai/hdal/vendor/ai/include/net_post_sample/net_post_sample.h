/**
	@brief Header file of definition of vendor net postprocessing sample.

	@file net_post_sample.h

	@ingroup net_post_sample

	@note Nothing.

	Copyright Novatek Microelectronics Corp. 2018.  All rights reserved.
*/
#ifndef _NET_POST_SAMPLE_H_
#define _NET_POST_SAMPLE_H_

/********************************************************************
	INCLUDE FILES
********************************************************************/
#include "hd_type.h"
#include "net_gen_sample/net_gen_sample.h"
#include "net_flow_sample/nn_net.h"
#include "net_flow_sample/nn_parm.h"

/********************************************************************
	MACRO CONSTANT DEFINITIONS
********************************************************************/
#define VENDOR_AIS_LBL_LEN      256     ///< maximal length of class label

/********************************************************************
	TYPE DEFINITION
********************************************************************/
/**
	Data dimensions.
*/
typedef struct _VENDOR_AIS_SHAPE {
	INT32 num;                          ///< Caffe blob: batch size
	INT32 channels;                     ///< Caffe blob: number of channels
	INT32 height;                       ///< Caffe blob: height
	INT32 width;                        ///< Caffe blob: width
	INT32 lineofs;                      ///< lineoffset: distance between two lines (rows); unit: byte
} VENDOR_AIS_SHAPE;

/**
    Output class.
*/
typedef struct _VENDOR_AIS_OUTPUT_CLASS {
	INT32 no;                           ///< class number
	FLOAT score;                        ///< class score
} VENDOR_AIS_OUTPUT_CLASS;

/**
	Parameters of accuracy calculation
*/
typedef struct _VENDOR_AIS_ACCURACY_PARM {
	UINT32 in_addr;                     ///< [in]  address of input data
	VENDOR_AIS_OUTPUT_CLASS *classes;   ///< [out] top scoring class list

	VENDOR_AIS_SHAPE shape;             ///< [in]  input/output data dimensions (height/width/lineofs is not used)
	INT32 top_n;                        ///< [in/out] number of top scoring classes

	INT32 *class_idx;                   ///< [in]  address of class index buffer
} VENDOR_AIS_ACCURACY_PARM;

/********************************************************************
	EXTERN VARIABLES & FUNCTION PROTOTYPES DECLARATIONS
********************************************************************/
extern HD_RESULT vendor_ais_softmax_process(NN_SOFTMAX_PARM  *p_parm);
extern HD_RESULT vendor_ais_accuracy_process(VENDOR_AIS_ACCURACY_PARM *p_parm);

#endif  /* _NET_POST_SAMPLE_H_ */
