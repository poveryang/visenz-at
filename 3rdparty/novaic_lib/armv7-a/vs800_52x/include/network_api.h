/*
 * @Author: yexiaoqi xiaoqi.ye@smartmore.com
 * @Date: 2023-07-21 11:00:55
 * @LastEditors: yexiaoqi xiaoqi.ye@smartmore.com
 * @LastEditTime: 2024-01-29 09:59:19
 * @Description:
 * Copyright (c) 2023 by yexiaoqi, All Rights Reserved.
 */

#ifndef NETWORK_API_H
#define NETWORK_API_H

#include "com_manager.h"
#include <memory>
#include <string>
#include <vector>
#include <cstring>

typedef struct
{
    HD_COMMON_MEM_INIT_CONFIG *cfg;
    int cfg_max_size;
    int (*get_index)(void);
} SM_NVT_AI_CONFIG_ST;

// /**
// 	@name input buffer / output buffer
// */
typedef struct _AI_OUT_BUF
{
    UINT32 va;        ///< virtual address
    UINT32 size;      ///< buffer size
    UINT32 width;     ///< width
    UINT32 height;    ///< height
    UINT32 channel;   ///< channel
    UINT32 batch_num; ///< number of batch
} AI_OUT_BUF;

typedef struct _MEM_PARM
{
    UINT32 pa;
    UINT32 va;
    UINT32 size;
    UINT32 blk;
} MEM_PARM;

struct NET_PROC_CONFIG
{
    std::string model_filename;
    INT32 binsize;

    NET_PROC_CONFIG(std::string filename, INT32 size, int job_method,
                    int wait_ms, int buf_method)
    {
        model_filename = filename;
        binsize = size;
        job_method_ = job_method;
        job_wait_ms_ = wait_ms;
        buf_method_ = buf_method;
    }

    //设置ai算子相关：具体见 AITG-NvtAI_SDK_Programing_Note_RevD 13
    void SetNetworkOpt(int job_method, int job_wait_ms, int buf_method)
    {
        if (job_method == 0 || job_method == 1 ||
            (job_method >= 10 && job_method <= 13))
        {
            job_method_ = job_method;
        }

        if (job_wait_ms >= -1)
        {
            job_wait_ms_ = job_wait_ms;
        }

        if (buf_method >= -1 && job_method <= 4)
        {
            buf_method_ = buf_method;
        }
    }

    int GetJobMethod() const { return job_method_; }
    int GetJobWaitMs() const { return job_wait_ms_; }
    int GetBufMethod() const { return buf_method_; }

    unsigned char *GetModelDataPtr() const { return model_data_; };
    void SetModelDataPtr(unsigned char *ptr) { model_data_ = ptr; };
    void CopyModelData(unsigned char *ptr, int size)
    {
        if (model_data_ != nullptr)
        {
            free(model_data_);
            model_data_ = nullptr;
        }

        model_data_ = static_cast<unsigned char *>(malloc(size));
        memcpy(model_data_, ptr, size);
    }

private:
    // 参考  AITG-NvtAI_SDK_Programing_Note_RevD 13
    int job_method_ = 11; /// 排程优化法： 0-1 循序 10-11 并行 默认11
    int job_wait_ms_ =
        0; /// ai 算子工作排程 -1:sync,0:async,>0 async with timeout
    int buf_method_ = 1; /// buf method:  0：不优化，1-4：lv0-4优化，-1：lv0优化

    unsigned char *model_data_ = nullptr;
};

struct NET_PROC
{
    NET_PROC_CONFIG net_cfg;
    MEM_PARM proc_mem;
    MEM_PARM rslt_mem;
    MEM_PARM io_mem;

    NET_PROC(UINT32 idx) : net_cfg("", 0, 1, 0, 1)
    {
        proc_id_ = idx;
        // net_cfg = NET_PROC_CONFIG("",0,11,0,1);
        proc_mem = {0};
        rslt_mem = {0};
        io_mem = {0};
        fair_core_ = 2;
    }

    UINT32 GetPathIdx() const { return proc_id_; }

    void SetFairCore(int idx) { fair_core_ = idx; };
    int GetFairCore() const { return fair_core_; };

private:
    UINT32 proc_id_;
    int fair_core_;
};

//网络公共内存参数
struct NetComMemParam
{
    UINT32 net_path_idx_count;
    std::vector<INT32> binsizes;
};

class NovaicNetWork
{
public:
    NovaicNetWork();
    ~NovaicNetWork();

    bool CheckModel(const std::string model_path);

    bool SetModelData(const std::string model_path, int model_size,
                      unsigned char *model_data);

    // 初始化网络
    bool InitNetwork();

    //运行网络
    bool RunNetwork();

    //关闭网络
    bool CloseNetwork();

    //获取推理结果
    std::vector<AI_OUT_BUF> GetOutBuffer() const { return out_buffer_; }

    //分配输入内存
    bool MallocInputBuffer(const std::array<int, 3> &input_img_size);

    //释放输入内存
    bool FreeInputBuffer();

    //输入图片
    bool SetInputBuffer(const uint8_t *input_img_data,
                        std::array<int, 3> input_img_size);

    //输入图片
    bool SetInputBufferWithSmv(const uint8_t *input_img_data,
                               std::array<int, 3> input_img_size);

    bool InputOpen(std::string file_name, int wid, int hei, int channel);

    static UINT32 GetNetCount() { return com_mem_parm_.net_path_idx_count; };
    static int GetNetBinsize(int idx) { return com_mem_parm_.binsizes[idx]; };
    static void SetNetBinsize(int idx, INT32 binzize)
    {
        com_mem_parm_.binsizes[idx] = binzize;
    };

    // 0->core_0,  1->core_1, 2->core_all
    void SetFairCore(int idx) { net_in_.SetFairCore(idx); };

    // static MEM_PARM image_mem_;

private:
    //网络公共内存参数
    static NetComMemParam com_mem_parm_;

    bool b_init_flag_ = true;
    bool b_set_input_buffer_ = true;

    NET_PROC net_in_;
    AI_BUF in_buffer_;
    std::vector<AI_OUT_BUF> out_buffer_;
    // protected:
    //     std::shared_ptr<CoManager> co_manager_ = nullptr;
};

/*
初始化共有mem,必须在所有模型实例都初始化后执行
*/
void InitComMem();

/*
 * npu 设置参数回调函数，使用函数指针。
 */
int SetNetworkConfig(HD_COMMON_MEM_INIT_CONFIG *cfg, int *index);

/*
 * npu 设置参数回调函数，使用结构体。
 */
int SetNetworkConfig(SM_NVT_AI_CONFIG_ST *ai_config);

// MEM_PARM GetImageMem();

#endif