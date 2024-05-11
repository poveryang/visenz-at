#ifndef __SM_VIDEO_CAPTURE__
#define __SM_VIDEO_CAPTURE__

#include <stdint.h>
#include "hd_common.h"
#include <stdbool.h>

#define SM_MSG(fmt, args...) \
    printf("%s:%d "fmt" \n",__func__, __LINE__, ##args)

#define IQ_CCM_LEN 9
#define IQ_CCM_HUETAB_LEN 24
#define IQ_CCM_SATTAB_LEN 24
#define MAX_PLANE_SIZE 4

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
   ERR_MAX
} SM_ERR_CODE;

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
    SM_VIDEO_CAPTURE, //驱动
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
        } nvt;
        struct {
            int     buf_index;
            void    *data;
        } nxp;
    } platform; //平台相关性
} __attribute__((packed)) SM_VIDEO_FRAME;

typedef struct sm_video_capture_frame {
    SM_VIDEO_FRAME sm_frame;
    HD_VIDEO_FRAME video_cap;
    HD_VIDEO_FRAME video_isp;
} SM_VIDEO_CAPTURE_FRAME;

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

/* sm_pic_enc
@ 参数1：enc 为输入的结构体指针
@ 返回: 0:成功，负数:失败号
*/
int sm_pic_enc(SM_ENC *enc);

/* get_mem_index
@ 返回: >= 0 当前未初始化使用的mem数组下标,-1可用空间已分配完
*/
int get_mem_index(void);

#endif
