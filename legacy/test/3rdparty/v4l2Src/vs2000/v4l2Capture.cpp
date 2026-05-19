/*******************************************************************************
 * FILENAME:      v4l2Capture.cpp
 *
 * AUTHORS:       raylei
 *
 * START DATE:    2022/02/21
 *
 * CONTACT:       xiangfeng.lei@smartmore.com

 *******************************************************************************/

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <stdlib.h>
#include <syslog.h>


#include "v4l2Capture.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define FOCUSTYPE_PATH "/sys/devices/platform/soc@0/30800000.bus/30a30000.i2c/i2c-1/1-0036/cmd"

int V4L2Capture::openDevice()
{
    printf("video dev : %s\n", devName);
    fdCam = open(devName, O_RDWR);
    if (fdCam < 0)
    {
        perror("Can't open video device");
        return -1;
    }
    set_fd_non_inheritable(fdCam);

    subfb = open(subName, O_RDWR);
    if (subfb < 0)
    {
        printf("Can't open %s device", subName);
        return -1;
    }
    set_fd_non_inheritable(subfb);
    openlog("vs2000", LOG_PID | LOG_PERROR, 0);

    return 0;
}

int V4L2Capture::closeDevice()
{

    closelog();
    if (fdCam > 0)
    {
        int ret = 0;
        if ((ret = close(fdCam)) < 0)
        {
            perror("Can't close video device");
        }
        else
        {
            fdCam = -1;
        }

		close(subfb);
		subfb = -1;
		
        return 0;
    }
    else
    {
        return -1;
    }
}

bool V4L2Capture::isOpening()
{
    if (fdCam < 0)
    {
        return false;
    }
    return true;
}

int V4L2Capture::initDevice(int frameRate, int width, int height, int imageBit)
{
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
	struct v4l2_streamparm sparm;
	unsigned int min;
    SM_LIGHT &light = SM_LIGHT::getInstance();
    light_current_version = light.getCurrentVersion();

	if (-1 == ioctl(fdCam, VIDIOC_QUERYCAP, &cap)) {
		fprintf(stderr, "VIDIOC_QUERYCAP: error - %d\n", errno);
		return -1;
	}

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
        fprintf(stderr, "%s is no video capture device\n",
                 devName);
		return -1;
    }

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "%s does not support streaming i/o\n",
			 devName);
		return -1;
	}

    
	if(setFrameRateFormat(frameRate, width, height, imageBit) < 0){
		return -1;
	}
	if(initBuffers() < 0){
		return -1;
	}
    focus_type = getLensVaild();

    startCapture();
    setStrobeEnable(1);
    usleep(300000);
    setStrobeEnable(0);	
    return 0;
}

int V4L2Capture::initBuffers()
{
	int i;
    int ret = 0;
    unsigned int n_buffers = 0;
    struct v4l2_buffer buf;
	struct v4l2_requestbuffers req;
    struct v4l2_plane mplanes[BUFFER_NUM];

	CLEAR(req);
	req.count = BUFFER_NUM;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	req.memory = memory_type;

	if (-1 == ioctl(fdCam, VIDIOC_REQBUFS, &req)) {
		fprintf(stderr, "VIDIOC_REQBUFS: error - %d\n", errno);
		return -1;
	}

	if (req.count < 2) {
		fprintf(stderr, "Insufficient buffer memory on %s\n",
		devName);
		return -1;
	}

    if (memory_type == V4L2_MEMORY_MMAP) {
        CLEAR(v4l2_buffers);
    	v4l2_buffers = (struct buffer *)calloc(req.count, sizeof(*v4l2_buffers));
    	if (!v4l2_buffers) {
	    	fprintf(stderr, "Out of memory\n");
		    return -1;
	    }

    	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {

    		CLEAR(buf);

		    buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    		buf.memory      = memory_type;
	    	buf.index       = n_buffers;
		    buf.m.planes	= mplanes;
    		buf.length	= NUM_PLANES;

	    	if (-1 == ioctl(fdCam, VIDIOC_QUERYBUF, &buf)) {
		    	fprintf(stderr, "VIDIOC_QUERYBUF: error - %d\n", errno);
			    return -1;
    		}

	    	for(int j=0; j<NUM_PLANES; j++) {
		    	v4l2_buffers[n_buffers].length[j] = buf.m.planes[j].length;
			    v4l2_buffers[n_buffers].start[j] = mmap(NULL, buf.m.planes[j].length,
    			PROT_READ | PROT_WRITE, /* recommended */
	    		MAP_SHARED,             /* recommended */
		    	fdCam, buf.m.planes[j].m.mem_offset);
		    	if (MAP_FAILED == v4l2_buffers[n_buffers].start[j]) {
			    	fprintf(stderr, "mmap: error - %d\n", errno);
			    	return -1;
			    }
		    }
	    }
    }

    if(memory_type == V4L2_MEMORY_USERPTR){
        CLEAR(userptr_buffers);
        for (int i = 0; i < BUFFER_NUM; i++){
            CLEAR(buf);
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            buf.memory = memory_type;
            buf.index = i;
            buf.m.planes = mplanes;
            buf.length = NUM_PLANES;
            if (ret = ioctl(fdCam, VIDIOC_QUERYBUF, &buf) < 0) {
                printf("VIDIOC_QUERYBUF %d failed: %d\n", i, errno);
                return -1;
            }
            userptr_buffers[i].length =  mWidth * mHeight;
            printf("buf.length = %d\n", buf.length);
        }
        memDev = sm_mem_dev_create(req.count,  userptr_buffers[0].length);
        sm_mem_alloc(memDev, req.count);
    }
	
	for (i = 0; i < BUFFER_NUM; ++i) {

		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory = memory_type;
		buf.index = i;
		buf.m.planes = mplanes;
		buf.length	= NUM_PLANES;

        if(memory_type == V4L2_MEMORY_USERPTR){
            userptr_buffers[i].phy = sm_mem_get_phyaddr(memDev,i);
            userptr_buffers[i].start = (void *)sm_mem_get_viraddr(memDev, i);
            buf.m.planes->m.userptr = userptr_buffers[i].phy;
        }

		if (-1 == ioctl(fdCam, VIDIOC_QBUF, &buf)) {
			fprintf(stderr, "VIDIOC_QBUF error: %d\n", errno);
			return -1;
		}
	}
	
	return 0;
}

int V4L2Capture::startCapture()
{
    enum v4l2_buf_type type;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if (-1 == ioctl(fdCam, VIDIOC_STREAMON, &type)) {
		fprintf(stderr, "VIDIOC_STREAMON error: %d\n", errno);
		return -1;
	}

    return 0;
}

int V4L2Capture::stopCapture()
{
    enum v4l2_buf_type type;
	
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (-1 == ioctl(fdCam, VIDIOC_STREAMOFF, &type))
    {
        printf("VIDIOC_STREAMOFF error\n");
        return -1;
    }

    return 0;
}

int V4L2Capture::freeBuffers()
{
	unsigned int i, j;

    if (memory_type == V4L2_MEMORY_USERPTR) {

        sm_mem_free(memDev, BUFFER_NUM);
        sm_mem_destroy(memDev);
        memDev = NULL;
    } else {	
    	for (i = 0; i < BUFFER_NUM; ++i) {
	    	for (j = 0; j < NUM_PLANES; j++) {
    			munmap(v4l2_buffers[i].start[j], v4l2_buffers[i].length[j]);
            }
	    }
        free(v4l2_buffers);
        v4l2_buffers = NULL;
    }
	return 0;
}

// timeout unit is us
static int timeDiffUs(const struct timeval &vStart, const struct timeval &vEnd)
{
    return (vEnd.tv_sec - vStart.tv_sec) * 1000000 + vEnd.tv_usec - vStart.tv_usec;
}

int V4L2Capture::getNewestFrame(void **frame_buf, size_t *len, int timeout, int runmode)
{
	int ret = 0;
	int reserved = 0;
	int full_state = 0;
#if 1
	if (runmode == 0){
		if (last_mode != 0)
			ret = setStrobeEnable(1);
		last_mode = runmode;
		for(int i=0;i<= BUFFER_NUM;i++){
			reserved = getFrame(frame_buf,len,timeout);
			if (reserved >= BUFFER_NUM -2){
				full_state = 1;
			}
			if (reserved > 0 || reserved == -1 ){
				backFrame();
			} else {
				if (full_state == 1 ){
					backFrame();
					if (getFrame(frame_buf, len, timeout) == -1)
					{
						printf("camera:get image failed,select timeout=%dms\n", timeout / 1000);
						return -1;
					}
				}
				getCurrentFrameSensorParam();
				if ( app_param.gain == cur_param.gain && app_param.exposure == cur_param.exposure && app_param.focus == cur_param.focus \
				&& app_param.lightBright[0] == cur_param.lightBright[0]){
					printf("v4l2getframe ok reserved =%d\n",reserved);
					return reserved;
				}else{
					backFrame();
				}
			}
		}
	} else {
		ret = setStrobeEnable(1);
		last_mode = runmode;
		for(int i=0;i<= BUFFER_NUM;i++){
			reserved = getFrame(frame_buf,len,timeout);
			if (reserved >= BUFFER_NUM -2){
				full_state = 1;
			}
			if (reserved > 0 || reserved == -1 ){
				backFrame();
			} else {
				if (full_state == 1 ){
					backFrame();
					if (getFrame(frame_buf, len, timeout) == -1)
					{
						printf("camera:get image failed,select timeout=%dms\n", timeout / 1000);
						return -1;
					}
				}
				getCurrentFrameSensorParam();
				if ( app_param.gain == cur_param.gain && app_param.exposure == cur_param.exposure && app_param.focus == cur_param.focus \
                && app_param.lightBright[0] == cur_param.lightBright[0]){
                    if (light_contrl != 1){
                        ret = setStrobeEnable(0);
                    }
					printf("v4l2getframe ok reserved =%d\n",reserved);
					return reserved;
				} else {
					backFrame();
				}
			}
		}
	}
	//超过十次采图都不对，判定为相机异常
	return 1;
#else
    struct timeval timeStart, timeEnd;
    int reserved = 0;
    int full_state = 0;
#define DEBUG_FRAME_TIME
#ifdef DEBUG_FRAME_TIME
    gettimeofday(&timeStart, nullptr);
#endif

    while (1)
    {
        reserved = getFrame(frame_buf, len, 1);
#ifdef DEBUG_FRAME_TIME
        printf("discard old frame... %d\n", reserved);
#endif
        // buffer is full.
        if (reserved >= BUFFER_NUM - 2)
        {
            printf("warnning: buffer is full, it will clear all and get newest frame\n");
            full_state = 1;
        }
        // get last frame in buffer queue and clear other frame. clear all when buffer is full.
        if (reserved > 0)
        {
            backFrame();
        }
        else
        {
        	/*discard all when buffer is fulll*/
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
#endif
}

int V4L2Capture::getFrame(void **frame_buf, size_t *len, int timeout)
{
	struct v4l2_buffer buf;
	struct v4l2_plane mplanes[NUM_PLANES];
	unsigned int i;

    fd_set fds;
    struct timeval tv;
    int res = 0;
	
	CLEAR(buf);

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	buf.memory = VB2_MEMORY_MMAP;
	buf.m.planes    = mplanes;
	buf.length      = NUM_PLANES;


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

    if (res == -1)
    {
        printf("V4L2Capture: select error 0x%x 0x%x\n", errno, EINTR);
        if (errno == EINTR)
        {
            return -1;
        }
    }
    else if (res == 0)
    {
        // printf("select timeout\n");
        return -1;
    }

    if (-1 == ioctl(fdCam, VIDIOC_DQBUF, &buf))
    {
        printf("VIDIOC_DQBUF error\n");
        return -1;
    }

    if (memory_type == V4L2_MEMORY_USERPTR) {
        *frame_buf = userptr_buffers[buf.index].start;
    } else {
        *frame_buf = v4l2_buffers[buf.index].start[0];
    }
	
    m_newframetime = buf.timestamp;
    *len = buf.m.planes->bytesused;
	frameIndex = buf.index;
    
    if (memory_type == V4L2_MEMORY_USERPTR) {
        sm_mem_buf_sync(memDev, frameIndex);
    }
	
    return buf.reserved;
}

int V4L2Capture::backFrame()
{
	struct v4l2_buffer buf;
	struct v4l2_plane mplanes[NUM_PLANES];
	unsigned int i;

	CLEAR(buf);

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	buf.memory = memory_type;
	buf.m.planes    = mplanes;
	buf.length      = NUM_PLANES;
	buf.index = frameIndex;

    if (memory_type == V4L2_MEMORY_USERPTR) {
        buf.m.planes->m.userptr = userptr_buffers[frameIndex].phy;
        buf.m.planes->length = userptr_buffers[frameIndex].length;
    }


	if (-1 == ioctl(fdCam, VIDIOC_QBUF, &buf)) {
		fprintf(stderr, "VIDIOC_QBUF error: %d\n", errno);
        return -1;
	}	
    return 0;
}

struct timeval V4L2Capture::getFrameDonetime()
{
    return m_newframetime;
}

#define FOCUSTYPE_PATH "/sys/devices/platform/soc@0/30800000.bus/30a30000.i2c/i2c-1/1-0036/cmd"
#define FOPENERR (-1)
#define FREADERR (-2)
#define VALERR (-3)

static int read_focus_type() {
    FILE *pfo  = NULL;
    size_t len = 0;
    char buf[16] = {0};
    int type = 0;
    int ret = 0;

    pfo = fopen(FOCUSTYPE_PATH, "r");
    if (pfo == NULL) { 
        syslog(LOG_ERR, "fopen err!");
        type = FOPENERR;
        pfo = NULL;
        goto end;
    }
    len = fread(buf, sizeof(char), 1, pfo);
    if (len < 1) {
        syslog(LOG_ERR, "fread err! len:%d\n", len);
        type = FREADERR;
        goto end;
    }
    buf[1] = '\0';
    ret = sscanf(buf, "%d", &type);
    if (ret != 1) {
        syslog(LOG_ERR, "sscanf failed! ret:%d\n", ret);
        type = VALERR;
        goto end;
    }
    // syslog(LOG_INFO, "buf:##%s## type:##%d##\n", buf, type);
end:
    if (pfo != NULL) {
        fclose(pfo);
    }
    return type;
}

int V4L2Capture::write_device_param(const char *data , int data_len) {
    FILE *pfo  = NULL;
    size_t len = 0;
    int ret = 0;

    syslog(LOG_INFO, "write_device_param---A--:##%s##  data_len:%d\n", data, data_len);
    syslog(LOG_INFO, "open write_device_param!");

    pfo = fopen(FOCUSTYPE_PATH, "w");
    if (pfo == NULL) { 
        syslog(LOG_ERR, "fopen err!");
        pfo = NULL;
        ret = -1;
        goto end;
    }
    else
    {
        syslog(LOG_ERR, "fopen ok!");
    }
    len = fwrite(data, sizeof(char), data_len, pfo);
    if (len < data_len) {
        syslog(LOG_ERR, "fwrite err! len:%ld\n", len);
        ret = -1;
        goto end;
    }
    else
    {
        syslog(LOG_INFO, "data---A--:##%s##\n", data);
    }

end:
    if (pfo != NULL) {
        fclose(pfo);
    }
    return ret;
}

//这个接口
int V4L2Capture::getLensVaild(void)
{
    int ret = 1;
    if ( (ret = read_focus_type()) < 0 ) {
        // syslog(LOG_INFO, "getLensVaild read_focus_type ret:%d\n", ret); 
        return 1;
    }
    // syslog(LOG_INFO, "getLensVaild read_focus_type ret:%d\n", ret); 
    return ret; 
#if 0
    struct v4l2_control ctrl = {V4L2_CID_FOCUS_ABSOLUTE, -1};
	int ret;
	
    ret = ioctl(subfb, VIDIOC_G_CTRL, &ctrl);
	if(ret < 0){
        syslog(LOG_INFO, "getLensVaild ioctl VIDIOC_G_CTRL ret:%d\n", ret); 
		return -1;
	}
    syslog(LOG_INFO, "getLensVaild ioctl:VIDIOC_G_CTRL cid:V4L2_CID_FOCUS_ABSOLUTE ctrl.value=%d\n", ctrl.value); 
    return ctrl.value;
#endif
}


int V4L2Capture::setGain(int value)
{

    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_GAIN;
    ctrl.value = value;
    gain = value;
	app_param.gain = value;
    int ret = ioctl(fdCam, VIDIOC_S_CTRL, &ctrl);

    return ret;
}

void V4L2Capture::LightStayOn(int enable)
{
	light_contrl = enable;
}

int V4L2Capture::setStrobeEnable(int enable)
{
    struct v4l2_control ctrl;
    SM_LIGHT &light = SM_LIGHT::getInstance();
    int ret =0;
    if(enable == 1){
        ctrl.id = V4L2_CID_FLASH_STROBE;
        // printf("light_current_version = %d ,mWidth = %d",light_current_version,mWidth);
        if(mWidth == 2448 || focus_type == 0){
        }
        else if(light_current_version == 1){
            // printf("ar2020 get right lightCurrentVersion\n");
            ret = light.setLightStatus(8, enable);
        }else{
            printf("ar2020 get wrong lightCurrentVersion\n");
            ctrl.id = V4L2_CID_FLASH_STROBE_STOP;
            ret = light.setLightStatus(8, 0);
        }
    }else{
        ctrl.id = V4L2_CID_FLASH_STROBE_STOP;
        if( mWidth != 2448 && light_contrl != 1){
            ret = light.setLightStatus(8, enable);
        }
    }
    ctrl.value = enable;
    ret = ioctl(fdCam, VIDIOC_S_CTRL, &ctrl);
    return ret;
}

int V4L2Capture::setExposure(int value)
{

    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_EXPOSURE;
    ctrl.value = value;
    exposure = value;
	app_param.exposure = value;
    int ret = ioctl(fdCam, VIDIOC_S_CTRL, &ctrl);

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

    //ctrl.id = V4L2_CID_FLASH_CHARGE;
	ctrl.id = V4L2_CID_FLASH_TIMEOUT;
	if(num > (LIGHT_NUM - 1)){
		return -1;
	}
    if(value < 0){
		value = 0;
	}
	if(value > LIGHT_BRIGHT_MAX){
		value = LIGHT_BRIGHT_MAX;
	}
    SM_LIGHT &light = SM_LIGHT::getInstance();
    if(mWidth == 2448 || focus_type == 0){
	    ret = light.setLightStatus(num, value);
    }
    else if(light_current_version == 1){
        // printf("ar2020 get right lightCurrentVersion\n");
	    ret = light.setLightStatus(num, value);
    }else{
        printf("ar2020 get wrong lightCurrentVersion\n");
        ctrl.id = V4L2_CID_FLASH_STROBE_STOP;
        ret = light.setLightStatus(8, 0);
    }

    ctrl.value = (num << 8) | value;
	app_param.lightBright[num] = value;
    ret |= ioctl(fdCam, VIDIOC_S_CTRL, &ctrl);
	if(ret < 0){
		printf("[%s %s %d error]\n", __FILE__, __func__, __LINE__);
	}
    return ret;
}


int V4L2Capture::setLightAim(int enable)
{
    struct v4l2_control ctrl;
	int ret;
	
    //ctrl.id = V4L2_CID_FLASH_CHARGE;
    ctrl.id = V4L2_CID_FLASH_TIMEOUT;	
    ctrl.value = (LIGHT_NUM << 8) | enable;
	
    ret = ioctl(fdCam, VIDIOC_S_CTRL, &ctrl);
	
	if(ret < 0){
		printf("[%s %s %d error]\n", __FILE__, __func__, __LINE__);
	}
    SM_LIGHT &light = SM_LIGHT::getInstance();
    ret = light.setLightAim(enable);
    return ret;
}

int V4L2Capture::setLensFocus(int value)
{
    struct v4l2_control ctrl;
	int ret;
	
    ctrl.id = V4L2_CID_FOCUS_ABSOLUTE;
    ctrl.value = value;
	app_param.focus = value;	
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
	int ret = 0;
	struct v4l2_format fmt;

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    ret = ioctl(fdCam, VIDIOC_G_FMT, &fmt);

	if (ret < 0)
	{
		fprintf(stderr, "vs2000 getformat err occurr! ret:%d\n", ret);
		return ret;
	}
	mWidth = fmt.fmt.pix_mp.width;
	mHeight = fmt.fmt.pix_mp.height;
	return ret;
}

/* 帧率 图片大小 设置  1280*800(30/60/120fps)  1280*720(60/120fps)*/
int V4L2Capture::setFrameRateFormat(int frameRate, int width, int height, int imageBit)
{
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	struct v4l2_streamparm sparm;
	
    int ret;

	mWidth = width;
	mHeight = height;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	fmt.fmt.pix_mp.width       = width; //replace
	fmt.fmt.pix_mp.height      = height; //replace
	fmt.fmt.pix_mp.field       = V4L2_FIELD_ANY;
	fmt.fmt.pix_mp.num_planes  = NUM_PLANES;
	fmt.fmt.pix_mp.plane_fmt[0].bytesperline  = width;
    if (imageBit == 8) {
        fmt.fmt.pix_mp.plane_fmt[0].sizeimage  = width * height;
        fmt.fmt.pix_mp.pixelformat = capFMT_8Bit;
    }else if(imageBit == 10){
        fmt.fmt.pix_mp.plane_fmt[0].sizeimage  = width * height * 2;
        fmt.fmt.pix_mp.pixelformat = capFMT_10Bit;
    }

    ret = ioctl(fdCam, VIDIOC_S_FMT, &fmt);  // 设置图片格式（宽高）
    if (ret < 0)
    {
        printf("Set picture format failed: ret=%d, %s", ret, strerror(errno));
        return -1;
    }

    memset(&sparm, 0, sizeof(struct v4l2_streamparm));
    sparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    sparm.parm.capture.timeperframe.denominator = frameRate;
    sparm.parm.capture.timeperframe.numerator = 1;
    ret = ioctl(fdCam, VIDIOC_S_PARM, &sparm);  // 设置帧率
    if (ret < 0)
    {
        printf("Set FrameRate failed: %s", strerror(errno));
        return -1;
    }
    printf("set FrameRate success, current frame rate = %d\n",
           sparm.parm.capture.timeperframe.denominator / sparm.parm.capture.timeperframe.numerator);


	
    printf("set picture format(width*height) success\n");

    return ret;
}

/* 帧率获取 */
int V4L2Capture::getFrameRate()
{
    int ret;
    int frameRate, denominator, numerator;

    struct v4l2_streamparm stream_p;
    memset(&stream_p, 0, sizeof(struct v4l2_streamparm));
    stream_p.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    ret = ioctl(fdCam, VIDIOC_G_PARM, &stream_p);
    if (ret < 0)
    {
        printf("Unable to get FrameRate: %s", strerror(errno));
        return -1;
    }
    denominator = stream_p.parm.capture.timeperframe.denominator;
    numerator = stream_p.parm.capture.timeperframe.numerator;
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
	
    struct v4l2_streamparm stream_p;
	
    memset(&stream_p, 0, sizeof(struct v4l2_streamparm));

	if(frameIndex < 0){
		return cur_param;
	}
	
    stream_p.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    ret = ioctl(fdCam, VIDIOC_G_PARM, &stream_p);
    if (ret < 0)
    {
        printf("v4l2:get sensor param failed: %s\n", strerror(errno));
        exit(1);
    }

    unsigned char *p = &stream_p.parm.raw_data[sizeof(struct v4l2_captureparm)];
    memcpy((void *)&cur_param, (void *)p, sizeof(struct sensorParam));

#if 1
    printf("v4l2 frameIndex = %d, paramActive = %d, gain = %d, exp=%d, bright=%d %d %d %d %d, focus=%d\n", 
		frameIndex, cur_param.active, cur_param.gain, cur_param.exposure, 
		cur_param.lightBright[0], cur_param.lightBright[1], cur_param.lightBright[2], cur_param.lightBright[3], cur_param.lightBright[4], cur_param.focus);
#endif
    return cur_param;
}

void V4L2Capture::setWidth(int vWidth)
{
    mWidth = vWidth;
}

int V4L2Capture::getWidth() const
{
    return mWidth;
}

void V4L2Capture::setHeight(int vHeight)
{
    mHeight = vHeight;
}

int V4L2Capture::getHeight() const
{
    return mHeight;
}

int V4L2Capture::getLightNumber(void)
{
    int ret = 0;
    SM_LIGHT &light = SM_LIGHT::getInstance();
    ret = light.getLightHWVersion();
    return ret;
}

light_color V4L2Capture::getLightColor(void)
{
    light_color color;
    SM_LIGHT &light = SM_LIGHT::getInstance();
    color = (light_color)light.getLightHWColor();
    return color;
}

char *V4L2Capture::getMcuVersion(char *buf)
{
    if (buf == NULL)
    {
        return NULL;
    }
    SM_LIGHT &light = SM_LIGHT::getInstance();
    return light.getMcuVersion(buf);
}

#define MAXVALUE_PATH "/sys/devices/platform/soc@0/30800000.bus/30a30000.i2c/i2c-1/1-0036/max"

static int read_max_value(const char *type, int type_len)
{
    FILE *pfo  = NULL;
    size_t len = 0;
    char buf[16] = {0};
    int ret = 0;
    int value = -1;

    pfo = fopen(MAXVALUE_PATH, "w");
    if (pfo == NULL) {
        syslog(LOG_ERR, "fopen err!");
        pfo = NULL;
        goto end;
    }

    len = fwrite(type, sizeof(char), type_len, pfo);
    if (len < type_len) {
        syslog(LOG_ERR, "fwrite err! len:%ld\n", len);
        goto end;
    }
    fclose(pfo);

    pfo = fopen(MAXVALUE_PATH, "r");
    if (pfo == NULL) {
        syslog(LOG_ERR, "fopen err!");
        pfo = NULL;
        goto end;
    }

    len = fread(buf, sizeof(buf), 1, pfo);
    if (strlen(buf) < 1) {
        syslog(LOG_ERR, "fread err! len:%ld\n", len);
        goto end;
    }

    buf[strlen(buf)] = '\0';
    ret = sscanf(buf, "%d", &value);
    if (ret != 1) {
        syslog(LOG_ERR, "sscanf failed! ret:%d\n", ret);
        goto end;
    }

end:
    if (pfo != NULL) {
        fclose(pfo);
    }
    return value;

}


int V4L2Capture::getMaxLensValue(void)
{
    char lens[20] =  "lens";

    return read_max_value(lens, strlen(lens));
}

int V4L2Capture::getMaxExposure(void)
{
    char exposure[20] = "exposure";

    return read_max_value(exposure, strlen(exposure));
}

int V4L2Capture::getMaxGain(void)
{
    char gain[20] = "gain";

    return read_max_value(gain, strlen(gain));
}

