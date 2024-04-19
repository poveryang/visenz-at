#pragma once

/*******************************************************************************
 * FILENAME:      v4l2Capture.h
 *
 * AUTHORS:       Shen Qi
 *
 * START DATE:    Thursday January 28th 2021
 *
 * CONTACT:       qi.shen@smartmore.com
 *******************************************************************************/

#include <unistd.h>
#include <linux/videodev2.h>
#include <iostream>
#include <mutex>
#include <unistd.h>
#include <fcntl.h>
#include "mem_manager.h"
#include "sm_light.h"

//#define USE_2M_SENSOR 
#define BUFFER_NUM				8

#define LIGHT_NUM				10

#define LIGHT_BRIGHT_MAX		24
//#define DMA_M2M_ENABLE 	

#define CLEAR(x) memset(&(x), 0, sizeof(x))


#define NUM_PLANES 1


#define SENSOR_CTRL_CMD_GET_FOCUS_STATUS		0
#define SENSOR_CTRL_CMD_SET_FOCUS				1
#define SENSOR_CTRL_CMD_SET_GAIN				2
#define SENSOR_CTRL_CMD_SET_EXPOSURE			3
#define SENSOR_CTRL_CMD_SET_LIGHT				4
#define SENSOR_CTRL_CMD_SET_STROBE				5


enum vb2_memory {
	VB2_MEMORY_UNKNOWN      = 0,
	VB2_MEMORY_MMAP         = 1,
};

typedef enum {
    HW_LIGHT_WHITE = 0,
    HW_LIGHT_RED,
    HW_LIGHT_BLUE,
}light_color;

struct sensorParam{
	unsigned int active; //参数生效标志
	unsigned int gain;
	unsigned int exposure;
	unsigned int focus;
	int focus_state;
	unsigned int lightBright[16];
};

struct cam_buffer
{
    void *start;
    unsigned int length;
    unsigned long phy;
};


class V4L2Capture
{
public:
	
    static V4L2Capture &getInstance()
    {
        static V4L2Capture instance;
        return instance;
    }

    V4L2Capture(){};
    ~V4L2Capture(){};
private:

    V4L2Capture(const V4L2Capture &) = delete;
    V4L2Capture &operator=(const V4L2Capture &) = delete;

public:
    int openDevice();
    int closeDevice();
    bool isOpening();
	int initDevice(int frameRate, int width, int height, int imageBit);
    int startCapture();
    int stopCapture();
    int freeBuffers();
    int getNewestFrame(void **frame_buf, size_t *len, int timeout, int runmode);
    struct sensorParam getCurrentFrameSensorParam();
    int getFrame(void **, size_t *, int timeout);
    int backFrame();
    struct timeval getFrameDonetime();

    // set gain and exposure should be done after start capture, otherwise failed
    int setGain(int value);
    int setExposure(int value);
    int setformat(int width, int height);
    int getformat();
    int getFrameRate();
    int setVerticalFlip(int value);
    int seHorizontalFlip(int value);
    int setCrop(int left, int top, int width, int height);
    void setWidth(int vWidth);
    int getWidth() const;
    void setHeight(int vHeight);
    int getHeight() const;
	int setStrobeEnable(int enable);
	int	setLightBright(int num, int value);
	int setLightAim(int enable);
	int setLensFocus(int value);
	int getLensVaild(void);
    int getLightNumber(void);
    light_color getLightColor(void);
    char *getMcuVersion(char *buf);
	int getMaxLensValue(void);
    int getMaxExposure(void);
    int getMaxGain(void);
	void LightStayOn(int enable);
    /**
     * @brief set_fd_non_inheritable, Set resources not to be occupied by processes
     * @param fd
     */
    void set_fd_non_inheritable(int fd)
    {
        int flags = fcntl(fd, F_GETFD);
        if (flags != -1)
        {
            fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
        }
    }

    int write_device_param(const char *data , int data_len);
    int fps = 30;

private:
    int initBuffers();
    int setFrameRateFormat(int frameRate, int width, int height, int imageBit);

	struct buffer {
		void   *start[NUM_PLANES];
		size_t	length[NUM_PLANES];
	};

    struct buffer *v4l2_buffers;
    struct sm_mem_dev *memDev;
    cam_buffer userptr_buffers[BUFFER_NUM];

    const char *devName = "/dev/video0";
    const char *subName = "/dev/v4l-subdev0";
    int light_current_version = -1 ;
#ifdef USE_2M_SENSOR

    int mWidth = 1600;
    int mHeight = 1300;
#else
    int mWidth = 2448;
    int mHeight = 2048;
#endif
    //const int capFMT = V4L2_PIX_FMT_NV12;//V4L2_PIX_FMT_SBGGR8;
    const int capFMT_8Bit = V4L2_PIX_FMT_SBGGR8;
    const int capFMT_10Bit = V4L2_PIX_FMT_SBGGR10;
    int gain = 16;
    int exposure = 2233;
    int sWidth = 0xFFFF;
	int last_mode = 0;

    int fdCam = -1;	
    int subfb = -1;    
    int frameIndex;
    int memory_type = V4L2_MEMORY_USERPTR; //V4L2_MEMORY_MMAP使用v4l2框架的拷贝 V4L2_MEMORY_USERPTR是使用sdma拷贝

    struct sensorParam app_param, cur_param;
    struct timeval m_newframetime;
    int focus_type = -1;
	int light_contrl = -1;	
    std::mutex mtx;
};
