/*
 * @Author: yexiaoqi xiaoqi.ye@smartmore.com
 * @Date: 2023-07-20 19:09:57
 * @LastEditors: yexiaoqi xiaoqi.ye@smartmore.com
 * @LastEditTime: 2024-01-31 19:07:10
 * @Description:
 * Copyright (c) 2023 by yexiaoqi, All Rights Reserved.
 */

#ifndef COMMOM_MANAGER_H
#define COMMOM_MANAGER_H

extern "C"
{
#include "util/hd_common.h"
}

typedef struct _AI_BUF
{
    UINT32 sign;                 ///< signature = MAKEFOURCC('A','B','U','F')
    UINT32 chunk_size;           ///< sizeof(this sturct) - 8;
    HD_COMMON_MEM_DDR_ID ddr_id; ///< ddr id
    UINT32 pa;                   ///< physical address
    UINT32 va;                   ///< virtual address
    UINT32 size;                 ///< buffer size
    HD_VIDEO_PXLFMT fmt;         ///< format
    UINT32 width;                ///< width
    UINT32 height;               ///< height
    UINT32 channel;              ///< channel
    UINT32 batch_num;            ///< number of batch
    UINT32 time;                 ///< number of time
    union
    {
        UINT32 reserve;    ///< reserved
        FLOAT scale_ratio; ///< for fixed and float
    };
    UINT32 line_ofs;    ///< line offset
    UINT32 channel_ofs; ///< channel offset
    UINT32 batch_ofs;   ///< batch offset
    UINT32 time_ofs;    ///< time offset
    CHAR layout[8];     ///< layout order of W,H,C,N
    CHAR *name;         ///< buffer name, eg: "mylayer.out0"
    CHAR *op_name;      ///< related operation name
} AI_BUF;

class CoManager
{
public:
    CoManager();
    ~CoManager();

    static void CommomUninit();

    static bool NetworkInit(void);
    static bool NetworkUninit(void);

    bool CommemInit();

    HD_COMMON_MEM_INIT_CONFIG *GetComMem() { return &mem_cfg_; }

private:
    void CommomInit();

    static bool b_commen_init_flag_;
    static bool b_commom_init_flag_;
    static bool b_network_init_flag_;

    HD_COMMON_MEM_INIT_CONFIG mem_cfg_;
};

#endif