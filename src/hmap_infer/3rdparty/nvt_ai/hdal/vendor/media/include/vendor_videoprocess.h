/**
	@brief Header file of vendor videoprocess module.\n
	This file contains the functions which is related to vendor videoprocess.

	@file vendor_videoprocess.h

	@ingroup mhdal

	@note Nothing.

	Copyright Novatek Microelectronics Corp. 2020.  All rights reserved.
*/

#ifndef _VENDOR_VIDEOPROCESS_H_
#define _VENDOR_VIDEOPROCESS_H_

/********************************************************************
	INCLUDE FILES
********************************************************************/
#include "hd_type.h"
#include "hd_common.h"
#include "hd_videoprocess.h"
/********************************************************************
	MACRO CONSTANT DEFINITIONS
********************************************************************/
#define HD_VIDEOPROC_PIPE_VPE 0x000000F2
#define HD_VIDEOPROC_PIPE_COMBINE       0x00001000
#define HD_VIDEOPROC_FUNC_DIRECT_SCALEUP 0x04000000 ///< enable scaling up in direct mode
/********************************************************************
	MACRO FUNCTION DEFINITIONS
********************************************************************/

/********************************************************************
	TYPE DEFINITION
********************************************************************/
typedef enum {
	VENDOR_VIDEOPROC_VPE_SCENE_DEFAULT = 0, //general case
	VENDOR_VIDEOPROC_VPE_SCENE_MF,          //mirror/flip
	VENDOR_VIDEOPROC_VPE_SCENE_WIDE,        //wide angle
	VENDOR_VIDEOPROC_VPE_SCENE_WALL,        //wall mount fish-eye
	VENDOR_VIDEOPROC_VPE_SCENE_EIS,         //eis
	VENDOR_VIDEOPROC_VPE_SCENE_DIS,         //dis
	VENDOR_VIDEOPROC_VPE_SCENE_ROT,	        //rot
	VENDOR_VIDEOPROC_VPE_SCENE_CEIL_FLOOR,  //ceil-floor fish-eye
	ENUM_DUMMY4WORD(VENDOR_VIDEOPROC_VPE_SCENE)
} VENDOR_VIDEOPROC_VPE_SCENE;

typedef enum _VENDOR_VIDEOPROC_PARAM_ID {
	VENDOR_VIDEOPROC_PARAM_HEIGHT_ALIGN,    ///< using device id, config output height align,only more buffer
	VENDOR_VIDEOPROC_PARAM_IN_DEPTH,	  	///< using in id
	VENDOR_VIDEOPROC_CFG_DIS_SCALERATIO,	///< config scale-ratio of DIS func (1100, 1200, 1400)
	VENDOR_VIDEOPROC_CFG_DIS_SUBSAMPLE,		///< config sub-sample of DIS func (0, 1, 2)
	VENDOR_VIDEOPROC_PARAM_STRIP,		    ///< set with ctrl path, set strip level.
	VENDOR_VIDEOPROC_PARAM_USER_CROP_TRIG,  ///< using path id, trigger user crop flow
	VENDOR_VIDEOPROC_PARAM_DIS_CROP_ALIGN,  ///< using out path, config dis crop addr align
	VENDOR_VIDEOPROC_PARAM_LINEOFFSET_ALIGN,///< using out path, config line offset align
	VENDOR_VIDEOPROC_PARAM_EIS_FUNC,        ///< using ctrl path, set TRUE for enabling EIS function.
	VENDOR_VIDEOPROC_PARAM_VPE_SCENE,       ///< using ctrl path, referring to VENDOR_VIDEOPROC_VPE_SCENE.
	ENUM_DUMMY4WORD(VENDOR_VIDEOPROC_PARAM_ID)
} VENDOR_VIDEOPROC_PARAM_ID;
/********************************************************************
	EXTERN VARIABLES & FUNCTION PROTOTYPES DECLARATIONS
********************************************************************/
HD_RESULT vendor_videoproc_set(UINT32 id, VENDOR_VIDEOPROC_PARAM_ID param_id, VOID *p_param);
#endif

