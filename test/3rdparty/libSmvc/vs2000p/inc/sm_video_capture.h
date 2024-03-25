#ifndef __SM_VIDEO_CAPTURE__
#define __SM_VIDEO_CAPTURE__

#include <stdint.h>
#include <stdbool.h>

#if (defined(PRODUCT_VS600) || defined(PRODUCT_VS800P) || defined(PRODUCT_VHS8000))
#include "hd_common.h"
#endif 


#define SM_MSG(fmt, args...) \
    printf("%s:%d "fmt" \n",__func__, __LINE__, ##args)

#define IQ_CCM_LEN 9
#define IQ_CCM_HUETAB_LEN 24
#define IQ_CCM_SATTAB_LEN 24
#define MAX_PLANE_SIZE 4
#define SM_AWB_TUNING_MWB_MAX 12

#if (defined(PRODUCT_VS600) || defined(PRODUCT_VS800P) || defined(PRODUCT_VHS8000))
/* SM_NVT_AI_CONFIG_ST 结构体, npu初始化相关内容
@ 成员1: cfg 为内存成员
@ 成员2: cfg_max_size 为内存池最大数量
@ 成员3: *get_index 函数指针，获取当前内存池分配的数量
*/
typedef struct {
    HD_COMMON_MEM_INIT_CONFIG *cfg;
    int cfg_max_size;
    int (*get_index)(void);
} SM_NVT_AI_CONFIG_ST;

/* AI_CONFIG 函数指针 
@ 参数1: config，传入结构体指针，用于 npu 初始化内存信息填充
@ 返回: 0:成功，负数:失败号
*/
typedef int (*AI_CONFIG)(SM_NVT_AI_CONFIG_ST *config);

/* AI_CLOSE 函数指针，在关闭内存之前关闭NPU资源
@ 返回: 0:成功，负数:失败号
*/
typedef bool (*AI_CLOSE)(void);
#endif

#ifndef ENUM_DUMMY4WORD 
#define ENUM_DUMMY4WORD(name) E_##name = 0x10000000
#endif

#ifndef UINT32
#define UINT32 uint32_t
#endif

#ifndef UINT8
#define UINT8 uint8_t
#endif 

typedef enum {
    SM_FMT_RAW8 = 0,
    SM_FMT_RAW10,
    SM_FMT_RAW12,
    SM_FMT_NV21,
    SM_FMT_NV12,
    SM_FMT_UYVY,
    SM_FMT_YUYV,
    ENUM_DUMMY4WORD(SM_FMT_TYPE)
} SM_FMT_TYPE;

typedef enum {
   SM_SUCCESS = 0,
   ERR_CAMERA_INIT = 1, //相机初始化
   ERR_MEM_INIT, //内存初始化
   ERR_MODULE_INIT, //模块初始化
   ERR_OPEN_MODULE, //模块初始化
   ERR_SET_CAPPARM, //设置采集参数
   ERR_START_CAP, //开启采集
   ERR_ALREAD_START, //该Sensor id已启动;
   ERR_CAMERA_UNINIT, //该Sensor 未启动;
   ERR_CAP_PULL_OUT, 
   ERR_MEM_BLK2PA, 
   ERR_MEM_MMAP, 
   ERR_PUSH_IN_PROC,
   ERR_PUSH_OUT_PRCO,
   ERR_START_PROC,
   ERR_NO_FREE_BUF, //没有空闲buf可用
   ERR_NO_USER, //count 为0：没有使用者或指针错误
   ERR_LIGHT_INIT, //闪光灯初始化
   ERR_GFX_INIT, //初始化GFX报错
   ERR_MAX
} SM_ERR_CODE;

typedef enum {
    SM_OV9281 = 0,
    SM_AR0144,
    SM_SC132GS,
    SM_AP1302,
    SM_AR0234,
    SM_AR0822,
    ENUM_DUMMY4WORD(SM_SENSOR_TYPE)
} SM_SENSOR_TYPE;

/* SM_ENC_TYPE 类型
@ 编码类型选择
*/
typedef enum {
    SM_JPGE = 0,
    SM_H264,
    SM_H265,
} SM_ENC_TYPE;

/* SM_ENC 结构体,编码传入的参数
@ 成员1: type 为编码类型
@ 成员2: ppic_in 为输入的图像内容
@ 成员3: ppic_out 为输出的图像内容
@ 成员4: pic_out_len 为输出的图像长度
@ 成员5: pic_int_len 为输入的图像长度
@ 成员6：为输入的编码图像质量，范围50~99
*/
typedef struct {
    SM_ENC_TYPE type;
    char *ppic_in;
    char *ppic_out;
    int  *pic_out_len;
    int  pic_in_len;
    int  pic_quality;
    int  width;
    int  height;
} SM_ENC;

typedef enum {
	SM_OP_TYPE_AUTO = 0,              ///< iq setting from manual_param
	SM_OP_TYPE_MANUAL,                ///< iq setting from auto_param, interpolation by gain, LV or CT
	SM_OP_TYPE_MAX,
    ENUM_DUMMY4WORD(SM_OP_TYPE)
} SM_OP_TYPE;

typedef struct {
    SM_OP_TYPE mode;
    int coef[IQ_CCM_LEN];                     ///< range : -1024~1023, 1X = 256, Color correction matrix [rr rg rb gr gg gb br bg bb], table size: 9
	int hue_tab[IQ_CCM_HUETAB_LEN];           ///< range : 0~255, color control hue adjust table, 128 : NO hue adjust, table size: 24
	int sat_tab[IQ_CCM_SATTAB_LEN];           ///< range : -128~127, color control sat adjust table, 0 : NO sat adjust, -128 : Min sat suppress, 127 : Max sat enhance, table size: 24
} SM_CCM;

// 当前使用者
typedef enum frame_owner {
    SM_VIDEO_CAPTURE = 0, //驱动
    SM_SCANNER, //下位机
    ALGORITHM, //算法
    ENUM_DUMMY4WORD(FRAME_OWNER)
} FRAME_OWNER;

typedef struct sm_video_frame {
    UINT32          total_size;                 //需要的数据大小
    FRAME_OWNER     owner;                      //使用者
    UINT8           *out_buf[MAX_PLANE_SIZE];   //图像 buf 指针
    int             buf_size[MAX_PLANE_SIZE];   //图像 buf 空间大小， get_frame() 的成功返回值
    int             plane_num;                  //通道总数
    int             count;                      //使用计数
    UINT32          sm_scanner_use_1;
    UINT32          sm_scanner_use_2;
    UINT32          sm_scanner_use_3;
    void            *priv;
    union {
        struct {
            int     buf_index;
            int     isp_status;
            UINT32  vir_addr_main; 
            UINT32  out_buf_phy[MAX_PLANE_SIZE];
        } nvt;
        struct {
            int     buf_index;
            void    *data;
        } nxp;
    } platform; //平台相关性
} __attribute__((packed)) SM_VIDEO_FRAME;

typedef struct sm_video_capture_frame {
    SM_VIDEO_FRAME sm_frame;
#if (defined(PRODUCT_VS600) || defined(PRODUCT_VS800P) || defined(PRODUCT_VHS8000))
    HD_VIDEO_FRAME video_cap;
    HD_VIDEO_FRAME video_isp;
#endif
    int            using_count;                      //使用计数
} SM_VIDEO_CAPTURE_FRAME;

typedef enum _SM_AWBT_ITEM {
	SM_AWBT_ITEM_VERSION            = 0,///< data_type: [Get]     UINT32
	SM_AWBT_ITEM_SIZE_TAB,              ///< data_type: [Get]     AWBT_INFO
	SM_AWBT_ITEM_RLD_CONFIG,            ///< data_type: [Set]     AWBT_CFG_INFO
	SM_AWBT_ITEM_RLD_DTSI,              ///< data_type: [Set]     AWBT_DTSI_INFO
	SM_AWBT_ITEM_RESERVE_04,
	SM_AWBT_ITEM_RESERVE_05,
	SM_AWBT_ITEM_RESERVE_06,
	SM_AWBT_ITEM_RESERVE_07,
	SM_AWBT_ITEM_RESERVE_08,
	SM_AWBT_ITEM_RESERVE_09,
	SM_AWBT_ITEM_SCENE             = 10,///< data_type: [Set/Get] AWBT_SCENE_MODE
	SM_AWBT_ITEM_WB_RATIO,              ///< data_type: [Set/Get] AWBT_WB_RATIO
	SM_AWBT_ITEM_OPERATION,             ///< data_type: [Set/Get] AWBT_OPERATION
	SM_AWBT_ITEM_STITCH_ID,             ///< data_type: [Set/Get] AWBT_STITCH_ID
	SM_AWBT_ITEM_RESERVE_14,
	SM_AWBT_ITEM_RESERVE_15,
	SM_AWBT_ITEM_RESERVE_16,
	SM_AWBT_ITEM_RESERVE_17,
	SM_AWBT_ITEM_RESERVE_18,
	SM_AWBT_ITEM_RESERVE_19,
	SM_AWBT_ITEM_RESERVE_20,
	SM_AWBT_ITEM_RESERVE_21,
	SM_AWBT_ITEM_RESERVE_22,
	SM_AWBT_ITEM_RESERVE_23,
	SM_AWBT_ITEM_RESERVE_24,
	SM_AWBT_ITEM_RESERVE_25,
	SM_AWBT_ITEM_RESERVE_26,
	SM_AWBT_ITEM_RESERVE_27,
	SM_AWBT_ITEM_RESERVE_28,
	SM_AWBT_ITEM_RESERVE_29,
	SM_AWBT_ITEM_CA_TH,           // 30,///< data_type: [Set/Get] AWBT_CA_TH
	SM_AWBT_ITEM_TH,                    ///< data_type: [Set/Get] AWBT_TH
	SM_AWBT_ITEM_LV,                    ///< data_type: [Set/Get] AWBT_LV
	SM_AWBT_ITEM_CT_WEIGHT,             ///< data_type: [Set/Get] AWBT_CT_WEIGHT
	SM_AWBT_ITEM_TARGET,                ///< data_type: [Set/Get] AWBT_TARGET
	SM_AWBT_ITEM_CT_INFO,         // 35,///< data_type: [Set/Get] AWBT_CT_INFO
	SM_AWBT_ITEM_MWB_GAIN,              ///< data_type: [Set/Get] AWBT_MWB_GAIN
	SM_AWBT_ITEM_CONVERGE,              ///< data_type: [Set/Get] AWBT_CONVERGE
	SM_AWBT_ITEM_EXPAND_BLOCK,          ///< data_type: [Set/Get] AWBT_EXPAND_BLOCK
	SM_AWBT_ITEM_MANUAL,                ///< data_type: [Set/Get] AWBT_MANUAL
	SM_AWBT_ITEM_STATUS,          // 40,///< data_type: [Get]     AWBT_STATUS
	SM_AWBT_ITEM_CA,                    ///< data_type: [Get]     AWBT_CA
	SM_AWBT_ITEM_FLAG,                  ///< data_type: [Get]     AWBT_FLAG
	SM_AWBT_ITEM_LUMA_WEIGHT,           ///< data_type: [Set/Get] AWBT_LUMA_WEIGHT
	SM_AWBT_ITEM_KGAIN_RATIO,           ///< data_type: [Set/Get] AWBT_ITEM_KGAIN_RATIO
	SM_AWBT_ITEM_CT_TO_CGAIN,     // 45,///< data_type: [Get]     AWBT_ITEM_CT_TO_CGAIN
	SM_AWBT_ITEM_RESERVE_46,
	SM_AWBT_ITEM_RESERVE_47,
	SM_AWBT_ITEM_RESERVE_48,
	SM_AWBT_ITEM_RESERVE_49,
	SM_AWBT_ITEM_MAX               = 50,
	ENUM_DUMMY4WORD(SM_AWBT_ITEM)
} SM_AWBT_ITEM;

typedef struct _SM_AWB_MANUAL {
    UINT32 en;
    UINT32 r_gain;
    UINT32 g_gain;
    UINT32 b_gain;
} SM_AWB_MANUAL;

typedef struct _SM_AWB_CA {
    UINT32 r_ca;
    UINT32 g_ca;
    UINT32 b_ca;
} SM_AWB_CA_ST;

/* SM_FLIP_TYPE 类型
@ 翻转类型选择
*/
typedef enum {
    SM_SEN_FLIP_TYPE_NONE = 0,  // Restores the unflipped state
    SM_SEN_FLIP_TYPE_H,         // X 
    SM_SEN_FLIP_TYPE_V,         // Y
    SM_SEN_FLIP_TYPE_H_V        // X 和 Y
} SM_FLIP_TYPE;

typedef enum sm_change_flag_type {
    SM_SET_CHANGE_FLAG_FLASH = 0,
    ENUM_DUMMY4WORD(SM_FLAG_TYPE)
} SM_FLAG_TYPE;

typedef enum e_sm_buzzer_status {
    SM_BUZZER_OFF= 0,
    SM_BUZZER_ON,
    SM_BUZZER_MAX,
    ENUM_DUMMY4WORD(SM_BUZZER_STATUS)
} SM_BUZZER_STATUS;

typedef struct sm_change_flag_value {
    int sm_change_flag_flash;
} SM_CHANGE_FLAG_VALUE;

/* sm_camera_init 
@ 参数1: 相机名称 （填NULL，选择ov9281)
@ 参数2: 分辨率宽（填0，选择默认1280)
@ 参数3: 分辨率高（填0，选择默认800)
@ 参数4: 图像格式 （填0， 选择RAW8）
@ 参数5: 帧率（填0,选择60）
@ 返回: 相机ID（正数）:成功 负数:失败号;
*/
int sm_camera_init(
    const char *sensor_name, 
    UINT32 width,
    UINT32 height,
    UINT32 fmt,
    UINT32 fps);


/* get_frame : 采图接口，会将指针交付给上层
@ 参数1： 相机ID
@ 参数2: 数据结构体，详情看 SM_VIDEO_FRAME 定义处注释;
@ 返回: frame的数据长度：成功，负数：失败号；
*/
int get_frame(int sid, SM_VIDEO_FRAME *pic_out);

/* free_frame ： 释放接口，将指针返还给驱动
@ 参数1： 相机ID
@ 参数2: 数据结构体，详情看 SM_VIDEO_FRAME 定义处注释;
@ 返回: 0:成功，负数：失败号；
*/
int free_frame(int sid, SM_VIDEO_FRAME *pic_out);

/* camera_uninit
@ 参数1：相机id；
@ 返回：0:成功，负数:失败号
*/
int sm_camera_uninit(
   int sid 
);
/* sm_get_focus_type 获取对焦类型
 *
 * Parameters:
@   sid: 相机ID
 * Returns: 0:手动 1:马达 2:液态
 *   
 */

int sm_get_focus_type(int sid);

/* sm_get_focus_max 获取对焦参数最大可设置值

 *
 * Parameters:
@   sid: 相机ID
 * Returns: 返回最大可设置的对焦值
 *   
 */
int sm_get_focus_max(int sid);

/* sm_get_focus_current 获取当前对焦参数

 *
 * Parameters:
@   sid: 相机ID
 * Returns: 当前对焦的对焦数值
 *   
 */

int sm_get_focus_current(int sid);

/* sm_set_focus_value 设置当前对焦的参数值

 *
 * Parameters:
@   sid: 相机ID
@   value: 对焦的值,有范围限制 [0 , sm_get_focus_max]
 * Returns: 0:成功 其他:错误码
 *   
 */

int sm_set_focus_value(int sid, int value);

/* sm_get_cur_gain
@ 返回：正数:成功(值为增益)，负数:失败号
*/
int sm_get_cur_gain(void);

/* sm_get_max_gain
@ 返回：正数:成功(值为增益)，负数:失败号
*/
int sm_get_max_gain(void);


/* sm_get_cur_expt 
@ 返回：正数:成功(值为曝光参数)，负数:失败号
*/
int sm_get_cur_expt(void);

/* sm_get_max_expt 
@ 返回：正数:成功(值为最大曝光参数)，负数:失败号
*/
int sm_get_max_expt(void);


/* sm_set_isp_ae_manual
@ expt: 曝光时间(单位:us)
@ gain: 增益系数(单位:根据sensor增益范围分配到0~255)
@ 返回: 0:成功，负数:失败号
*/
int sm_set_isp_ae_manual(int expt, int gain);

/* sm_set_isp_ae_auto
@ 返回: 0:成功，负数:失败号
*/
int sm_set_isp_ae_auto(void);

/* sm_set_light_state
@ state: 0:关灯，其他:开灯
@ 返回: 0:成功，负数:失败号
*/
int sm_set_light_state(int state);

/* sm_set_aimlight_state
@ 返回: 0:成功，负数:失败号
*/
int sm_set_aimlight_state(int state);
/* sm_set_aimlight_mutex
@ 返回: 0:成功，负数:失败号
*/
int sm_set_aimlight_mutex(int state);

/* sm_set_ccm_arg
@ 参数1：mode，标定模式（SM_OP_TYPE_AUTO，SM_OP_TYPE_MANUAL）
@ 参数1：ccm，颜色标定ccm矩阵
@ 参数1：ccm_size，ccm矩阵大小
@ 返回：0:成功，负数:失败号
*/
int sm_set_ccm_arg(SM_OP_TYPE mode, SM_CCM* sm_ccm, size_t ccm_size);

/* sm_isp_open
@ 返回：0:成功，负数:失败号
*/
int sm_isp_open(void);

/* sm_isp_close
@ 返回：0:成功，负数:失败号
*/
int sm_isp_close(void);

#if (defined(PRODUCT_VS600) || defined(PRODUCT_VS800P) || defined(PRODUCT_VHS8000))
/* sm_set_ai_config
@ 参数1:func 为设置npu内存的配置函数
@ 返回: 0:成功，负数:失败号
*/
int sm_set_ai_config(AI_CONFIG func);

/* sm_set_ai_close
@ 参数1:func 为设置npu关闭的函数
@ 返回: 0:成功，负数:失败号
*/
int sm_set_ai_close(AI_CLOSE func);
#endif

/* sm_pic_enc
@ 参数1：enc 为输入的结构体指针
@ 返回: 0:成功，负数:失败号
*/
int sm_pic_enc(SM_ENC *enc);

/* 运动侦灵敏度定义 */
typedef enum sm_md_sensitivity {
    SM_MD_SENSITIVITY_LOW = 0,  //低
    SM_MD_SENSITIVITY_MID,      //中
    SM_MD_SENSITIVITY_HIG,      //高
    SM_MD_SENSITIVITY_MAX,
    ENUM_DUMMY4WORD(SM_MD_SENSITIVITY_E)
} SM_MD_SENSITIVITY_E;

typedef struct sm_md_param {
    int sid;
    int sensitivity; // 0:低、1:中、2:高
    int detect_timeout; //感应超时
    int global_threshold; //全局判断的阈值,0-100,越小敏感
} SM_MD_PARAM;

/** 感应功能初始化
* @p：动态侦测模块初始化参数
* @返回: 0:成功 x:代表错误码
*/
int sm_md_init(SM_MD_PARAM *p);

/** 感应功能反初始化
* @p：动态侦测模块初始化参数
* @返回: 0:成功 x:代表错误码
*/
int sm_md_uninit(SM_MD_PARAM *p);

/** 感应功能开启
* @sid：sensor ID
* @返回: 0:成功 x:代表错误码
*/
int sm_md_start(int sid);

/** 感应功能关闭
* @sid：sensor ID
* @返回: 0:成功 x:代表错误码
*/
int sm_md_stop(int sid);

/** 感应功能参数配置
* @sid：sensor ID
* @返回值：0（成功），其他（错误码）
*/
int sm_md_set_param(int sid, SM_MD_PARAM *p);

/** 当前感应功能参数获取
* @sid：sensor ID
* @返回值：0（成功），其他（错误码）
*/
int sm_md_get_param(int sid, SM_MD_PARAM *p);


/** 注册监听函数
* @sid：sensor ID
* @callback：侦测触发回调函数
* @data：用户传入数据（方便异步上下文使用）
* @返回: 0:成功 x:代表错误码
*/
int sm_md_register_cb(int sid, int (*callback)(int sid, void *data), void *data);

/** 反注册监听函数
* @sid：sensor ID
* @返回: 0:成功 x:代表错误码
*/
int sm_md_unregister_cb(int sid);

/* get_mem_index
* @返回: >= 0 当前未初始化使用的mem数组下标,-1可用空间已分配完
*/
int get_mem_index(void);

/* sm_flash_colour_ctl
*  @colour: colour num: 4 位二进制分别控制；对应位 1 为开，0 为关
*                       1       1       1       1
*                       red2    red1    blue    white
*       example：   0x1:red2, 0x2:red1, 0x4:blue, 0x8:while; 0x3:red1 and red2, 0x5:red2 and blue, 0x9: red2 and white
* @返回: 0:设置成功； 负数：失败
*/
int sm_flash_colour_ctl(int colour);

/* sm_flip_ctl
* @sid：sensor ID
*  @status:  0：恢复不翻转；1：X翻转；2：Y翻转；3：X和Y翻转。
* @返回: 0:设置成功； 负数：失败
*/
int sm_flip_ctl(int sid, SM_FLIP_TYPE status);

/* sm_isp_get_awb
* @item: SM_AWBT_ITEM
* @p_param: 需要填充的结构体指针，需要传入对应的结构体指针。
* @返回: 0:设置成功； 负数：失败
*/
int sm_isp_get_awb(SM_AWBT_ITEM item, void* param);

/* sm_isp_set_awb
* @item: SM_AWBT_ITEM
* @p_param: 需要设置的结构体指针。
* @返回: 0:设置成功； 负数：失败
*/
int sm_isp_set_awb(SM_AWBT_ITEM item, void* param);

/* sm_pmemcpy
* @pa_dst：待拷贝的目的物理地址
* @pa_src:  数据源地址, 必须是物理地址
* @返回: 0:拷贝成功；-1: 拷贝失败
*/
int sm_pmemcpy(uint32_t pa_dst, uint32_t pa_src, int size);


/* sm_memcpy
* @pa_dst：待拷贝的目的物理地址
* @va:  数据源地址,必须是从驱动获取的图像虚拟地址才支持
* @返回: 0:拷贝成功；-1: va地址无效，非驱动分配; -2：拷贝失败
*/
int sm_memcpy(uint32_t pa_dst, void *va, int size);

/* sm_va2pa
* @va:  数据源地址,必须是从驱动获取的图像虚拟地址才支持
* @返回: 物理地址数值:拷贝成功；0xffffffff: va地址无效，非驱动分配;
*/
uint32_t sm_va2pa(void *va);

int sm_get_test_mem(int size, UINT32 *pa, UINT32 *va, int *blk);
int sm_free_test_mem(int size, UINT32 pa, UINT32 va, int blk);

/* sm_prepare_light_state_vn4000cl 
* @top_left: 上左灯的开关状态，1:open 0:close
* @top_right: 上右灯的开关状态，1:open 0:close
* @返回: 大于0的数用于sm_set_light_state接口的输入值state;
*/
int sm_prepare_light_state_vn4000cl(int top_left, int top_right, int bottom_left, int bottom_right);

/* sm_isp_set_gamma 
* @sid：sensor的id值, 通过camera_init返回的
* @gamma: 待设置的gamma值
* @返回: 0:成功；-1: 失败
*/
int sm_isp_set_gamma(int sid, uint32_t gamma);

/* sm_dev_set_buzzer_status 
* @sid：sensor的id值, 通过camera_init返回的
* @status: 待设置的蜂鸣器状态值 SM_BUZZER_OFF:关闭蜂鸣器 SM_BUZZER_ON:开启蜂鸣器
* @返回: 0:成功；-1: 失败
*/
int sm_dev_set_buzzer_status(int sid, SM_BUZZER_STATUS status);


/* sm_prepare_light_state_rk3588 
* @num: 灯对应的位置
* @state: 灯的状态（0:关灯、1:开灯）
* @返回: 大于0的数用于sm_set_light_state接口的输入值state;
*/
int sm_prepare_light_state_rk3588(int num, int state);


/* sm_get_light_num_rk3588 
* @返回: 机器对应的闪光灯的数量 4/6表示灯的数量，小于0表示机器未识别到灯板
*/
int sm_get_light_num_rk3588(void);


#endif
