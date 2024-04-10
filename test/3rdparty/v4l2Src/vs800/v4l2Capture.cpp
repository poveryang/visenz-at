/*******************************************************************************
 * FILENAME:      v4l2Capture.cpp
 *
 * AUTHORS:       raylei
 *
 * START DATE:    2022/02/21
 *
 * CONTACT:       xiangfeng.lei@smartmore.com
 *******************************************************************************/

#include "v4l2Capture.h"
#include "rpmsg.h"
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

int V4L2Capture::openDevice()
{
    Rpmsg& rpmsg_instance = Rpmsg::getInstance();
    rpmsg_instance.camera_power(1);
    usleep(50000);
//    printf("video dev : %s\n", devName);
    fdCam = open(devName, O_RDWR);
    if (fdCam < 0){
        perror("Can't open video device");
    }

    return 0;
}

int V4L2Capture::closeDevice()
{
	int ret = 0;

    if (fdCam > 0){
        if ((ret = close(fdCam)) < 0){
            perror("Can't close video device");
        }
        else{
            fdCam = -1;
        }
        Rpmsg& rpmsg_instance = Rpmsg::getInstance();
        rpmsg_instance.camera_power(0);
        return 0;
    }
    else{
        return -1;
    }
}

bool V4L2Capture::isOpening()
{
    if (fdCam < 0){
        return false;
    }
    return true;
}

int V4L2Capture::initDevice(int frameRate, int width, int height, int bit)
{
    int ret;
    struct v4l2_capability cam_cap;   //显示设备信息
    struct v4l2_cropcap cam_cropcap;  //设置摄像头的捕捉能力
    struct v4l2_fmtdesc cam_fmtdesc;  //查询所有支持的格式：VIDIOC_ENUM_FMT
    struct v4l2_crop cam_crop;        //图像的缩放
    struct v4l2_format cam_format;    //设置摄像头的视频制式、帧格式等

    ret = ioctl(fdCam, VIDIOC_QUERYCAP, &cam_cap);
    if (ret < 0) {
        perror("Can't get device information: VIDIOCGCAP");
		
		return -1;
    }

//    printf("Driver Name:%s\nCard Name:%s\nBus info:%s\nDriver Version:%u.%u.%u\n", cam_cap.driver, cam_cap.card,
//           cam_cap.bus_info, (cam_cap.version >> 16) & 0XFF, (cam_cap.version >> 8) & 0XFF, cam_cap.version & 0XFF);

    /* 使用IOCTL命令VIDIOC_ENUM_FMT，获取摄像头所有支持的格式*/
	getformat();


    /* 使用IOCTL命令VIDIOC_S_FMT，设置摄像头帧信息*/

	mWidth = width, 
	mHeihgt = height;
	
	ret = setFrameRateFormat(frameRate, mWidth, mHeihgt, bit);
    if (ret < 0){
        perror("frame format set error");
		return -1;
    }

    /* 使用IOCTL命令VIDIOC_G_FMT，获取摄像头帧信息*/
    cam_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fdCam, VIDIOC_G_FMT, &cam_format);
    if (ret < 0){
        perror("Can't get frame information");
    }

//    printf("Current data format information:\n\twidth:%d\n\theight:%d\n", cam_format.fmt.pix.width,
//           cam_format.fmt.pix.height);

    ret = initBuffers();
    if (ret < 0){
        perror("Buffers init error");
		return -1;
    }
    return 0;
}

int V4L2Capture::initBuffers()
{
    /* 使用IOCTL命令VIDIOC_REQBUFS，申请帧缓冲*/
    struct v4l2_requestbuffers req;
	struct v4l2_buffer buf;
    int ret;
	int i;
	
    CLEAR(req);
    req.count = BUFFER_NUM;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = memory_type;
    ret = ioctl(fdCam, VIDIOC_REQBUFS, &req);

    if (ret < 0){
        perror("Request frame buffers failed");
    }

	if(memory_type == V4L2_MEMORY_MMAP){
		for (i = 0; i < req.count; i++){
			
			CLEAR(buf);
			// 查询序号为n_buffers 的缓冲区，得到其起始物理地址和大小
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = memory_type;
			buf.index = i;
			ret = ioctl(fdCam, VIDIOC_QUERYBUF, &buf);
			if (ret < 0) {
				printf("VIDIOC_QUERYBUF %d failed\n", i);
				return -1;
			}
		
			buffers[i].length = buf.length;
			buffers[i].start = mmap(NULL,  buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fdCam, buf.m.offset);
		
			if (MAP_FAILED == buffers[i].start) {
				perror("mmap buffer%d failed\n");
				return -1;
			}
		}

		for (i = 0; i < req.count; i++){
			
			CLEAR(buf);
		
			// 查询序号为n_buffers 的缓冲区，得到其起始物理地址和大小
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.flags = V4L2_BUF_FLAG_MAPPED;
			buf.memory = memory_type;
			buf.index = i;
			ret = ioctl(fdCam, VIDIOC_QUERYBUF, &buf);
			buffers[i].phy = buf.m.offset;
		}		
	}

	if(memory_type == V4L2_MEMORY_USERPTR){
		for (i = 0; i < req.count; i++){
			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = memory_type;
			buf.index = i;
			ret = ioctl(fdCam, VIDIOC_QUERYBUF, &buf);
			if (ret < 0) {
				printf("VIDIOC_QUERYBUF %d failed\n", i);
				return -1;
			}
			buffers[i].length = buf.length;
		}
		memDev = sm_mem_dev_create(req.count,  buffers[0].length);
		sm_mem_alloc(memDev, req.count);
	}
	
    for (i = 0; i < req.count; i++){
		
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = memory_type;
        buf.index = i;

		if(memory_type == V4L2_MEMORY_USERPTR){
            buf.length = buffers[i].length;
            buffers[i].phy = sm_mem_get_phyaddr(memDev,i);
            buffers[i].start = (void *)sm_mem_get_viraddr(memDev, i);
			buf.m.userptr = buffers[i].phy;
		}
        if (-1 == ioctl(fdCam, VIDIOC_QBUF, &buf)) {
            printf("VIDIOC_QBUF buffer%d failed\n", i);
            return -1;
        }
    }
    return 0;
}

int V4L2Capture::startCapture()
{

    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == ioctl(fdCam, VIDIOC_STREAMON, &type)) {
        printf("VIDIOC_STREAMON e rror");
        return -1;
    }

    return 0;
}

int V4L2Capture::stopCapture()
{
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctl(fdCam, VIDIOC_STREAMOFF, &type)){
        printf("VIDIOC_STREAMOFF error\n");
        return -1;
    }

    return 0;
}

int V4L2Capture::freeBuffers()
{
    unsigned int i;

	if(memory_type == V4L2_MEMORY_USERPTR){
		sm_mem_free(memDev, BUFFER_NUM);
		sm_mem_destroy(memDev);
		
	}else{
		for (i = 0; i < BUFFER_NUM; ++i){
			if (-1 == munmap(buffers[i].start, buffers[i].length)){
				printf("munmap buffer%d failed\n", i);
				return -1;
			}
		}
	}
    return 0;
}

// timeout unit is us
static int timeDiffUs(const struct timeval &vStart, const struct timeval &vEnd)
{
    return (vEnd.tv_sec - vStart.tv_sec) * 1000000 + vEnd.tv_usec - vStart.tv_usec;
}

int V4L2Capture::getNewestFrame(void **frame_buf, size_t *len, int timeout)
{
    struct timeval timeStart, timeEnd;
    int reserved = 0;
    int full_state = 0;
//#define DEBUG_FRAME_TIME
#ifdef DEBUG_FRAME_TIME
    gettimeofday(&timeStart, nullptr);
#endif

    while (1)
    {
        reserved = getFrame(frame_buf, len, 10);
#ifdef DEBUG_FRAME_TIME
//        printf("discard old frame... %d\n", reserved);
#endif
        // buffer is full.
        if (reserved >= BUFFER_NUM - 2)
        {
//            printf("warnning: buffer is full, it will clear all and get newest frame\n");
            full_state = 1;
        }
        // get last frame in buffer queue and clear other frame. clear all when buffer is full.
        if (reserved > 0)
        {
            backFrame();
        }
        else
        {
        	/*discard all when buffer is full*/
			if(full_state == 1){
				reserved = -1;
				backFrame();
			}
		
            break;
        }
    }
    if (reserved == -1)
    {
        if (getFrame(frame_buf, len, timeout) == -1)
        {
            printf("camera:get image failed,select timeout=%dms\n", timeout / 1000);
            return -1;
        }
    }

#ifdef DEBUG_FRAME_TIME
    gettimeofday(&timeEnd, nullptr);
//    printf("get frame time =%dms state=%d\n", timeDiffUs(timeStart, timeEnd) / 1000, full_state);
#endif
    return 0;
}

struct timeval V4L2Capture::getFrameDonetime()
{
    return m_newframetime;
}

int V4L2Capture::getFrame(void **frame_buf, size_t *len, int timeout)
{
    struct v4l2_buffer queue_buf;
    fd_set fds;
    struct timeval tv;
    int res = 0;

    CLEAR(queue_buf);
    queue_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    queue_buf.memory = memory_type;

    FD_ZERO(&fds);
    FD_SET(fdCam, &fds);
	
    // timeout
    if(timeout < -1){
		res = select(fdCam + 1, &fds, NULL, NULL, NULL);
	}else{
		tv.tv_sec = timeout / 1000000;
		tv.tv_usec = timeout % 1000000;
		res = select(fdCam + 1, &fds, NULL, NULL, &tv);
	}
	
    if (res == -1){
        printf("V4L2Capture: select error 0x%x 0x%x\n", errno, EINTR);
        //if (errno == EINTR)
        {
            return -1;
        }
    }else if (res == 0){
       //  printf("V4L2Capture: select timeout\n");
        return -1;
    }
	
    if (-1 == ioctl(fdCam, VIDIOC_DQBUF, &queue_buf)){
        printf("VIDIOC_DQBUF error\n");
        return -1;
    }

    m_newframetime = queue_buf.timestamp;
    frameIndex = queue_buf.index;
    *len = buffers[frameIndex].length;
    *frame_buf = buffers[queue_buf.index].start;

	if(memory_type == V4L2_MEMORY_USERPTR){
		sm_mem_buf_sync(memDev, frameIndex);
	}

    return queue_buf.reserved;
}

/*************************************************
*@note: paramActive is 0 when gain exposure and light bright is changed.  paramActive is 1 after frame irq 
*@return: 0:not active,  1:active
**************************************************/

int V4L2Capture::backFrame()
{
    if (frameIndex != -1)
    {
        struct v4l2_buffer queue_buf;
        CLEAR(queue_buf);

        queue_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        queue_buf.memory = memory_type;
        queue_buf.index = frameIndex;
		
		if(memory_type == V4L2_MEMORY_USERPTR){
			queue_buf.m.userptr = buffers[frameIndex].phy;
			queue_buf.length = buffers[frameIndex].length;
		}
        if (-1 == ioctl(fdCam, VIDIOC_QBUF, &queue_buf)){
            printf("VIDIOC_QBUF error\n");
            return -1;
        }

        return 0;
    }

    return -1;
}

/*
int V4L2Capture::setStreamEnable(int enable)
{
	int ret;

    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_SATURATION;
    ctrl.value = enable;
    ret = ioctl(fdCam, VIDIOC_S_CTRL, &ctrl);
	if(ret < 0){
		printf("[%s %s %d error]\n", __FILE__, __func__, __LINE__);
	}
    return ret;
}
*/

int V4L2Capture::setStrobeEnable(int enable)
{
	int ret;

    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_FLASH_STROBE_STOP;
    ctrl.value = enable;
    ret = ioctl(fdCam, VIDIOC_S_CTRL, &ctrl);
	if(ret < 0){
		printf("[%s %s %d error]\n", __FILE__, __func__, __LINE__);
	}

    Rpmsg& rpmsg_instance = Rpmsg::getInstance();
    rpmsg_instance.setLightStatus(1, enable == 0 ? 0 :light_status);

    return ret ;
}

/*************************************************
*@param: return: -1 : lens invaild ; 0/1: vaild
**************************************************/

int V4L2Capture::getLensVaild(void)
{
    struct v4l2_control ctrl;
	int ret;
	
    ctrl.id = V4L2_CID_FOCUS_ABSOLUTE;

    ret = ioctl(fdCam, VIDIOC_G_CTRL, &ctrl);
	if(ret < 0){
		return -1;
	}
    return ctrl.value;
}

/*************************************************
*@param: return: -1 : lens invaild ; 0: vaild
**************************************************/

int V4L2Capture::getLensTemperature(void)
{
    struct v4l2_control ctrl;
	int ret;

	if(getLensVaild() < -1){
		return -1;
	}
	
    ctrl.id = V4L2_FLASH_FAULT_OVER_TEMPERATURE;

    ret = ioctl(fdCam, VIDIOC_G_CTRL, &ctrl);
	if(ret < 0){
		return -1;
	}
    return ctrl.value;
}


/*************************************************
*@param: value: 16~256
**************************************************/

int V4L2Capture::setGain(int value)
{
	int ret;

    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_GAIN;
    ctrl.value = value;
    gain = value;
	
    ret = ioctl(fdCam, VIDIOC_S_CTRL, &ctrl);
	if(ret < 0){
		printf("[%s %s %d error]\n", __FILE__, __func__, __LINE__);
	}
    return ret;
}


/*************************************************
*@param: value: 20us~1s
*@note:  帧率随曝光时间改变，例如:1fps@1s, 最大帧率为60pfs
**************************************************/

int V4L2Capture::setExposure(int value)
{
	int ret;

    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_EXPOSURE;
    ctrl.value = value;
    exposure = value;
    ret = ioctl(fdCam, VIDIOC_S_CTRL, &ctrl);
	if(ret < 0){
		printf("[%s %s %d error]\n", __FILE__, __func__, __LINE__);
	}
    return ret;
}

/*************************************************
*@param: value: 10 ~ 2000
**************************************************/
int V4L2Capture::setLightMaxTime(int value)
{
	int ret;

    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_FLASH_TIMEOUT;

	if(value < 10){
		ctrl.value = 10;
	}else{
		ctrl.value = value;
	}
	
    ret = ioctl(fdCam, VIDIOC_S_CTRL, &ctrl);
	ret = setExposure(exposure);
	
	if(ret < 0){
		printf("[%s %s %d error]\n", __FILE__, __func__, __LINE__);
	}

	
    return ret;
}

int V4L2Capture::setLightEnable(int enable)
{
    struct v4l2_control ctrl;
	int ret;
	
    ctrl.id = V4L2_CID_FLASH_CHARGE;
    ctrl.value = enable;
	
    ret = ioctl(fdCam, VIDIOC_S_CTRL, &ctrl);
	if(ret < 0){
		printf("[%s %s %d error]\n", __FILE__, __func__, __LINE__);
	}

    Rpmsg& rpmsg_instance = Rpmsg::getInstance();
    rpmsg_instance.setLightStatus(1, enable == 0 ? 0 :light_status);

    return ret;
}


/*************************************************
*@brief: set light brightness
*@param: num: 0~3, value:0~24
**************************************************/
int V4L2Capture::setLightBright(int num, int value)
{
    struct v4l2_control ctrl;
	int ret;
	
    ctrl.id = V4L2_CID_FLASH_CHARGE;

	if(num > (LIGHT_NUM - 1)){
		return -1;
	}
	
	if(value < 0){
		value = 0;
	}
	if(value > LIGHT_BRIGHT_MAX){
		value = LIGHT_BRIGHT_MAX;
	}

    ctrl.value = (num << 24) | value;
	
    ret = ioctl(fdCam, VIDIOC_S_CTRL, &ctrl);
	if(ret < 0){
		printf("[%s %s %d error]\n", __FILE__, __func__, __LINE__);
	}

    Rpmsg& rpmsg_instance = Rpmsg::getInstance();
    ret = rpmsg_instance.setLightStatus(num, value);
    light_status = value;
    return ret >= 0 ? 0 : -1;
}


int V4L2Capture::setLightAim(int enable)
{
    struct v4l2_control ctrl;
	int ret;
	
    ctrl.id = V4L2_CID_FLASH_CHARGE;
	
    ctrl.value = (LIGHT_NUM << 24) | enable;
	
    ret = ioctl(fdCam, VIDIOC_S_CTRL, &ctrl);
	
	if(ret < 0){
		printf("[%s %s %d error]\n", __FILE__, __func__, __LINE__);
	}

    Rpmsg& rpmsg_instance = Rpmsg::getInstance();
    ret = rpmsg_instance.setLightAim(enable);
    printf("[%s %s %d enable = %d]\n", __FILE__, __func__, __LINE__, enable);

    return ret;
}

int V4L2Capture::setLensFocus(int value)
{
    struct v4l2_control ctrl;
	int ret;
	
    ctrl.id = V4L2_CID_FOCUS_ABSOLUTE;
	
    ctrl.value = value;
	
    ret = ioctl(fdCam, VIDIOC_S_CTRL, &ctrl);
	
	if(ret < 0){
		printf("[%s %s %d error]\n", __FILE__, __func__, __LINE__);
	}

    return ret;
}

int V4L2Capture::setVerticalFlip(int value)
{
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_VFLIP;
    ctrl.value = value;
    int ret = ioctl(fdCam, VIDIOC_S_CTRL, &ctrl);
    if (ret < 0)
    {
        printf("setVerticalFlip failed, ret=%d\n", ret);
        return ret;
    }

    return ret;
}

int V4L2Capture::seHorizontalFlip(int value)
{
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_HFLIP;
    ctrl.value = value;
    int ret = ioctl(fdCam, VIDIOC_S_CTRL, &ctrl);
    if (ret < 0)
    {
        printf("seHorizontalFlip failed, ret=%d\n", ret);
        return ret;
    }

    return ret;
}
/*
/ follow step:
/ set fmt->(if stream on,must be stream off)-> set crop-> set fmt-> stream on
/
/ left,top,width,height must be 16byte alignment
*/
int V4L2Capture::setCrop(int left, int top, int width, int height)
{
    int ret;

    struct v4l2_selection sel;
    sel.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    sel.target = V4L2_SEL_TGT_CROP;
    sel.r.top = top;
    sel.r.left = left;
    sel.r.width = width;
    sel.r.height = height;
    ret = ioctl(fdCam, VIDIOC_S_SELECTION, &sel);
    if (ret < 0)
    {
        printf("%s failed, ret=%d\n", __func__, ret);
        return ret;
    }

    /*

        struct v4l2_crop crop;
        printf("%s %d\n",__func__, __LINE__);
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c.top = top;
        crop.c.left = left;
        crop.c.width = width;
        crop.c.height = height;
         ret = ioctl(fdCam, VIDIOC_S_CROP, &crop);
        if(ret < 0){
            printf("%s failed, ret=%d\n", __func__, ret);
            return ret;
        }
      */
    return ret;
}

/* 获取内核中支持的图片格式 */
int V4L2Capture::getformat()
{
    struct v4l2_fmtdesc cam_fmtdesc;  //查询所有支持的格式：VIDIOC_ENUM_FMT

    /* 使用IOCTL命令VIDIOC_ENUM_FMT，获取摄像头所有支持的格式*/
    cam_fmtdesc.index = 0;
    cam_fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    printf("Support format:\n");

    while (ioctl(fdCam, VIDIOC_ENUM_FMT, &cam_fmtdesc) != -1)
    {
        printf("getformat() i=%d , camera format= %s\n", cam_fmtdesc.index + 1, cam_fmtdesc.description);
        cam_fmtdesc.index++;
    }
}

/*************************************************
*@param: frameRate:1~60, width:1280, height:800
**************************************************/

int V4L2Capture::setFrameRateFormat(int frameRate, int width, int height, int bit)
{
    int ret;
    struct v4l2_streamparm Stream_Parm;
    struct v4l2_format format;
    enum v4l2_buf_type type;

    memset(&Stream_Parm, 0, sizeof(struct v4l2_streamparm));
    Stream_Parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    Stream_Parm.parm.capture.timeperframe.denominator = frameRate;
    Stream_Parm.parm.capture.timeperframe.numerator = 1;
    ret = ioctl(fdCam, VIDIOC_S_PARM, &Stream_Parm);  // 设置帧率
    if (ret < 0)
    {
        printf("Set FrameRate failed: %s", strerror(errno));
        return -1;
    }
    printf("set FrameRate success, current frame rate = %d\n",
           Stream_Parm.parm.capture.timeperframe.denominator / Stream_Parm.parm.capture.timeperframe.numerator);

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
	if(bit == 8){
		format.fmt.pix.pixelformat = capFMT8;
	}else if(bit == 10){
		format.fmt.pix.pixelformat = capFMT10;
	}else{
		printf("set frame bit =  %d failed!!!\n", bit);
		return -1;
	}	

    format.fmt.pix.field = V4L2_FIELD_NONE;  // MUST set

    ret = ioctl(fdCam, VIDIOC_S_FMT, &format);  // 设置图片格式（宽高）
    if (ret < 0)
    {
        printf("Set image format failed: ret=%d, %s", ret, strerror(errno));
        return -1;
    }
    printf("set image format success\n");

    return ret;
}

/* 帧率获取 */
int V4L2Capture::getFrameRate()
{
    int ret;
    int frameRate, denominator, numerator;

    struct v4l2_streamparm Stream_Parm;
    memset(&Stream_Parm, 0, sizeof(struct v4l2_streamparm));
    Stream_Parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ret = ioctl(fdCam, VIDIOC_G_PARM, &Stream_Parm);
    if (ret < 0)
    {
        printf("Unable to get FrameRate: %s", strerror(errno));
        return -1;
    }
    denominator = Stream_Parm.parm.capture.timeperframe.denominator;
    numerator = Stream_Parm.parm.capture.timeperframe.numerator;
    frameRate = denominator / numerator;

    printf("get FrameRate = %d\n", frameRate);
    return frameRate;
}
//此函数获取getFrame（dqbuf）后对应FrameIndex的buffer参数。
struct sensorParam V4L2Capture::getCurrentFrameSensorParam()
{
    int ret;
    int frameRate, denominator, numerator;
	int i;
	
    struct v4l2_streamparm Stream_Parm;
	
    memset(&Stream_Parm, 0, sizeof(struct v4l2_streamparm));

	if(frameIndex < 0){
		return sparam;
	}
	
    Stream_Parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ret = ioctl(fdCam, VIDIOC_G_PARM, &Stream_Parm);
    if (ret < 0){
        printf("v4l2:get sensor param failed: %s\n", strerror(errno));
        return sparam;
    }

    unsigned char *p = &Stream_Parm.parm.raw_data[sizeof(struct v4l2_captureparm)];
    memcpy((void *)&sparam, (void *)p, sizeof(struct sensorParam));

#if 1
    printf("v4l2 frameIndex = %d, paramActive = %d, gain = %d, exp=%d, bright=%d %d %d %d %d, focus=%d\n", 
		frameIndex, sparam.active, sparam.gain, sparam.exposure, 
		sparam.lightBright[0], sparam.lightBright[1], sparam.lightBright[2], sparam.lightBright[3], sparam.lightBright[4], sparam.focus);
#endif
    return sparam;
}

int V4L2Capture::getWidth()
{
    return mWidth;
}


int V4L2Capture::getHeight()
{
    return mHeihgt;
}


