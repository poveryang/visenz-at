/**
	@brief Header file of definition of vendor net preprocessing sample.

	@file net_pre_sample.h

	@ingroup net_pre_sample

	@note Nothing.

	Copyright Novatek Microelectronics Corp. 2018.  All rights reserved.
*/
#ifndef _NET_PRE_SAMPLE_H_
#define _NET_PRE_SAMPLE_H_

/********************************************************************
	INCLUDE FILES
********************************************************************/
#include "hd_type.h"
#include "net_preproc.h"
#include "net_flow_sample/net_flow_sample.h"

/********************************************************************
	TYPE DEFINITION
********************************************************************/

#define VENDOR_AIS_MAX_PLANE_NUM                3

/**
	Parameters of JPEG-to-RGB
*/
typedef struct _VENDOR_AIS_JPG2RGB_PARM {
	CHAR *jpg_filename;                 ///< JPEG file name
	UINT32 jpg_pa;                      ///< pyhsical address of JPEG input data
	UINT32 jpg_va;                      ///< virtual address of JPEG input data
	UINT32 dec_pa;                      ///< pyhsical address of decoder buffer
	UINT32 dec_va;                      ///< virtual address of decoder buffer
	UINT32 rgb_pa;                      ///< pyhsical address of RGB output data
	UINT32 rgb_va;                      ///< virtual address of RGB output data
} VENDOR_AIS_JPG2RGB_PARM;

/**
	Parameters of JPEG-to-YUV
*/
typedef struct _VENDOR_AIS_JPG2YUV_PARM {
	CHAR *jpg_filename;                 ///< JPEG file name
	UINT32 jpg_pa;                      ///< pyhsical address of JPEG input data
	UINT32 jpg_va;                      ///< virtual address of JPEG input data
	UINT32 dec_pa;                      ///< pyhsical address of decoder buffer
	UINT32 dec_va;                      ///< virtual address of decoder buffer
	UINT32 yuv_pa;                      ///< pyhsical address of YUv output data
	UINT32 yuv_va;                      ///< virtual address of YUV output data
} VENDOR_AIS_JPG2YUV_PARM;

typedef struct _VENDOR_AIS_IMG_PARM {
	UINT32 pa;
    UINT32 va;
	HD_VIDEO_PXLFMT fmt;                ///< image format
	UINT16 width;                       ///< image width
	UINT16 height;                      ///< image height
	UINT16 channel;                     ///< image channels
	UINT16 batch_num;                   ///< batch processing number
	UINT32 line_ofs;                    ///< line offset
	UINT32 channel_ofs;                 ///< channel offset
	UINT32 batch_ofs;                   ///< batch offset
#if AI_SUPPORT_MULTI_FMT
	UINT32 fmt_type;                    ///< format type
#endif
} VENDOR_AIS_IMG_PARM;

typedef struct _VENDOR_AIS_PRE_BUFF {
	VENDOR_AIS_FLOW_MEM_PARM src;
	VENDOR_AIS_FLOW_MEM_PARM dst;
	VENDOR_AIS_FLOW_MEM_PARM interm;
	VENDOR_AIS_FLOW_MEM_PARM mean;
} VENDOR_AIS_PRE_BUFF;

typedef struct _VENDOR_AIS_IMG_PARM_V2 {
	UINT32 img_ch_num;
	VENDOR_AIS_IMG_PARM* img_ch; 
} VENDOR_AIS_IMG_PARM_V2;
/**
    image bin file header
*/
//@{
/*typedef struct _NET_IMG_BIN
{
	HD_VIDEO_PXLFMT fmt;                ///< image format
	UINT16 width;                       ///< image width
	UINT16 height;                      ///< image height
	UINT16 channel;                     ///< image channels
	UINT16 batch_num;                   ///< batch processing number
	UINT32 line_ofs;                    ///< line offset
	UINT32 channel_ofs;                 ///< channel offset
	UINT32 batch_ofs;                   ///< batch offset
} NET_IMG_BIN;*/
//@}

/********************************************************************
	EXTERN VARIABLES & FUNCTION PROTOTYPES DECLARATIONS
********************************************************************/
//extern HD_RESULT vendor_ais_jpg2rgb_process(VENDOR_AIS_JPG2RGB_PARM *p_parm);
extern HD_RESULT vendor_ais_jpg2yuv_process(VENDOR_AIS_JPG2YUV_PARM *p_parm, VENDOR_AIS_IMG_PARM *p_oimg);

extern HD_RESULT vendor_ais_pre_init(VOID);
extern HD_RESULT vendor_ais_pre_uninit(VOID);
extern HD_RESULT vendor_ais_preproc(NN_PRE_PARM *p_pre_parm, VENDOR_AIS_PRE_BUFF buff, UINT32 net_id);

#endif  /* _NET_PRE_SAMPLE_H_ */
