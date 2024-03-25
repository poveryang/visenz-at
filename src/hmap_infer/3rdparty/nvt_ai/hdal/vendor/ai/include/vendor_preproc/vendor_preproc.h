#ifndef _VENDOR_AI_PREPROC_H_
#define _VENDOR_AI_PREPROC_H_

/********************************************************************
	INCLUDE FILES
********************************************************************/
#include "kdrv_ai.h"

/********************************************************************
	MACRO CONSTANT DEFINITIONS
********************************************************************/
#define AI_PREPROC_INPUT_CROP_EN    0x00000001      ///< preprocessing input crop enable
#define AI_PREPROC_TRANS_FMT_EN     0x00000002      ///< preprocessing tranform format enable
#define AI_PREPROC_SCALE_EN         0x00000004      ///< preprocessing scaling enable
#define AI_PREPROC_SUB_EN           0x00000008      ///< preprocessing substraction enable
#define AI_PREPROC_SCL_CROP_EN      0x00000010      ///< preprocessing scaling crop enable
#define AI_PREPROC_PAD_EN           0x00000020      ///< preprocessing padding enable
#define AI_PREPROC_FLIP_EN          0x00000040      ///< preprocessing flip enable
#define AI_PREPROC_ROTATE_EN        0x00000080      ///< preprocessing rotate enable

#define PREPROC_INTERFACE               1
#define AI_PREPROC_MAX_IO_NUM 3
/********************************************************************
	TYPE DEFINITION
********************************************************************/
#if PREPROC_INTERFACE
/**
    support engine for preprocess
*/
//@{   
typedef enum {
	AI_PREPROC_ENG_UNKNOWN     = 0,     
	AI_PREPROC_ENG_NUE2        = 1,
	ENUM_DUMMY4WORD(AI_PREPROC_ENG)
} AI_PREPROC_ENG;

/**
    preproc channel format
*/
//@{   
typedef enum {
	AI_PREPROC_CH_Y      = 0,     
	AI_PREPROC_CH_R      = 0,
	AI_PREPROC_CH_G      = 1,
	AI_PREPROC_CH_UVPACK = 1,
	AI_PREPROC_CH_B      = 2,	
	ENUM_DUMMY4WORD(AI_PREPROC_CH_FMT)
} AI_PREPROC_CH_FMT;

/**
    source format for preprocessing
*/
//@{   
typedef enum {
	AI_PREPROC_FMT_YUV420      = 0,     ///< source YUV420 format
	AI_PREPROC_FMT_YONLY       = 1,     ///< source YONLY format
	AI_PREPROC_FMT_UVPACK      = 2,     ///< source UVPACK format
	AI_PREPROC_FMT_RGB         = 3,     ///< source RGB format
	AI_PREPROC_FMT_YUV420_NV21 = 4,     ///< source YUV420_NV21 format
	ENUM_DUMMY4WORD(AI_PREPROC_FMT)
} AI_PREPROC_FMT;

/**
    select input/output type
*/
//@{
typedef enum {
	AI_PREPROC_TYPE_INT8          = 0,
	AI_PREPROC_TYPE_UINT8         = 1,
	AI_PREPROC_TYPE_INT16         = 2,
	AI_PREPROC_TYPE_UINT16        = 3,
	AI_PREPROC_TYPE_INT12,    
	AI_PREPROC_TYPE_UINT12,
	AI_PREPROC_TYPE_INT32,
	AI_PREPROC_TYPE_UINT32,
	AI_PREPROC_TYPE_INT9,    
	AI_PREPROC_TYPE_UINT9, 	
	ENUM_DUMMY4WORD(AI_PREPROC_TYPE)
} AI_PREPROC_TYPE;
//@}

/**
    format transform type
*/
//@{
typedef enum {
	AI_PREPROC_YUV2RGB          = 0,
	AI_PREPROC_RGB2HSV          = 1, 
	AI_PREPROC_YUV2HSV          = 2,
	ENUM_DUMMY4WORD(AI_PREPROC_TRANS_FMT_MODE)
} AI_PREPROC_TRANS_FMT_MODE;
//@}

/**
    substraction mode
*/
//@{
typedef enum {
	AI_PREPROC_SUB_SCALAR       = 0,
	AI_PREPROC_SUB_PLANAR       = 1,
	ENUM_DUMMY4WORD(AI_PREPROC_SUB_MODE)
} AI_PREPROC_SUB_MODE;
//@}

/**
    size configuration
*/
//@{
typedef struct _AI_PREPROC_SIZE {
	UINT16 width;
	UINT16 height;
	UINT16 channel;
} AI_PREPROC_SIZE;
//@}

/**
    offset configuration
*/
//@{
typedef struct _AI_PREPROC_OFS {
	UINT32 line_ofs;                ///< line offset for multi-stripe mode or image mode
	UINT32 channel_ofs;             ///< channel offst for multi-stripe mode
	UINT32 batch_ofs;               ///< batch offset for batch mode
} AI_PREPROC_OFS;
//@}

/**
    offset configuration
*/
//@{
typedef struct _AI_PREPROC_IO_INFO {
	UINT32 pa;
	UINT32 va;
	AI_PREPROC_TYPE type;
	AI_PREPROC_SIZE size;
	AI_PREPROC_OFS  ofs;
} AI_PREPROC_IO_INFO;
//@}

/**
    scaling parameters configuration
*/
//@{	
typedef struct _AI_PREPROC_SCALE_PARM {
	AI_PREPROC_SIZE scl_out_size;     ///< output size
 } AI_PREPROC_SCALE_PARM;
//@}

/**
    substraction parameters configuration
*/
//@{	
typedef struct _AI_PREPROC_SUB_PARM {
	AI_PREPROC_SUB_MODE mode;                ///< substraction mode
	UINT32 sub_val[AI_PREPROC_MAX_IO_NUM];   ///< substraction value  (for dc mode)
	AI_PREPROC_IO_INFO sub_planar;           ///< substraction planar (for planar mode)
 } AI_PREPROC_SUB_PARM;
//@}

/**
    crop parameters configuration
*/
//@{	
typedef struct _AI_PREPROC_CROP_PARM {
	UINT32 crop_x;                           ///< starting X of cropping
	UINT32 crop_y;                           ///< starting Y of cropping
	UINT32 crop_w;                           ///< width of cropping
    UINT32 crop_h;                           ///< height of cropping
 } AI_PREPROC_CROP_PARM;
//@}

/**
    padding parameters configuration
*/
//@{	
typedef struct _AI_PREPROC_PAD_PARM {
    UINT32 pad_out_x;                        ///< starting X of padding after crop
    UINT32 pad_out_y;                        ///< starting Y of padding after crop
    UINT32 pad_out_w;                        ///< width of output
    UINT32 pad_out_h;                        ///< height of output
    UINT32 pad_val[AI_PREPROC_MAX_IO_NUM];   ///< padding value
} AI_PREPROC_PAD_PARM;
//@}

/**
    format transform parameters configuration
*/
//@{	
typedef struct _AI_PREPROC_TRANS_FMT_PARM {
    AI_PREPROC_TRANS_FMT_MODE mode;          ///< format transform mode
} AI_PREPROC_TRANS_FMT_PARM;
//@}

/**
    flip parameters configuration
*/
//@{	
typedef struct _AI_PREPROC_FLIP_PARM {
    BOOL x_flip_en;
	BOOL y_flip_en;
} AI_PREPROC_FLIP_PARM;
//@}

/**
    flip parameters configuration
*/
//@{	
typedef struct _AI_PREPROC_ROTATE_PARM {
    BOOL   rotate_dir;
	UINT32 rotate_angle;
} AI_PREPROC_ROTATE_PARM;
//@}


/**
    preprocessing parameters configuration
*/
//@{   
typedef struct _AI_PREPROC_PARM {
    UINT32 func_en;
	AI_PREPROC_FMT src_fmt;                ///< input source format
	AI_PREPROC_FMT rst_fmt;                ///< output source format
    AI_PREPROC_IO_INFO input[AI_PREPROC_MAX_IO_NUM];
    AI_PREPROC_IO_INFO output[AI_PREPROC_MAX_IO_NUM];
    AI_PREPROC_CROP_PARM      input_crop_parm;		///< input crop parameters
    AI_PREPROC_TRANS_FMT_PARM trans_fmt_parm;       ///< format transform parameters
    AI_PREPROC_SCALE_PARM     scale_parm;           ///< scaling parameters
	AI_PREPROC_SUB_PARM       sub_parm;             ///< substraction parameters
    AI_PREPROC_CROP_PARM      scl_crop_parm;		///< scaling crop parameters
    AI_PREPROC_PAD_PARM       pad_parm;             ///< padding parameters
    AI_PREPROC_FLIP_PARM      flip_parm;			///< flip parameters
    AI_PREPROC_ROTATE_PARM    rotate_parm;          ///< rotate parameters
	
} AI_PREPROC_PARM;
//@}

#endif

INT32 vendor_preproc_init_parm(AI_PREPROC_PARM *p_parm);
INT32 vendor_preproc_set_io_parm(AI_PREPROC_PARM *p_parm, VOID* p_input_parm, AI_PREPROC_FMT fmt, UINT32 io_type, AI_PREPROC_CH_FMT io_idx);
INT32 vendor_preproc_set_func_parm(AI_PREPROC_PARM *p_parm, VOID* p_input_parm, UINT32 func_type);
INT32 vendor_preproc_run(AI_PREPROC_PARM *p_param, UINT32* run_flg);
INT32 vendor_preproc_chk_done(AI_PREPROC_ENG eng_type);
INT32 vendor_preproc_init(VOID);
INT32 vendor_preproc_uninit(VOID);
INT32 vendor_preproc_auto_alloc_mem(UINT32 buf_pa, UINT32 buf_va, KDRV_AI_PREPROC_INFO* p_preproc_mem);
UINT32 vendor_preproc_get_mem_size(VOID);
INT32 vendor_preproc_run_v2(KDRV_AI_PREPROC_INFO* p_preproc_mem, AI_PREPROC_PARM *p_param, UINT32* run_flg);

#endif
