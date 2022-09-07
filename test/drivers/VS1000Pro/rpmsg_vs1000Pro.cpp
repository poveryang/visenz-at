#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include "rpmsg_vs1000Pro.h"
#include "sys/time.h"

int Rpmsg::get_diff_time(struct timeval timeStart, struct timeval timeEnd)
{
	return (timeEnd.tv_sec - timeStart.tv_sec) * 1000000 + (timeEnd.tv_usec - timeStart.tv_usec);
}

int Rpmsg::initRpmsg(void)
{
    if (fd_rpmsg > 0)
        return -1;

    fd_rpmsg = open(rpmsg_name, O_RDWR | O_NOCTTY );   //O_RDWR | O_NOCTTY
    if (fd_rpmsg < 0)
    {
        perror(rpmsg_name);
        return -1;
    } 
    tcflush(fd_rpmsg, TCIOFLUSH);
    struct termios ti;
    if (tcgetattr(fd_rpmsg, &ti) < 0) {
		printf("error %d\r\n", fd_rpmsg);
		return -1;
	}
    cfmakeraw(&ti);
	cfsetospeed(&ti, B115200);
	cfsetispeed(&ti, B115200);
	if (tcsetattr(fd_rpmsg, TCSANOW, &ti) < 0) {
		printf("error %d\r\n", fd_rpmsg);
		return -1;
	}
    tcflush(fd_rpmsg,TCIOFLUSH);

    pthread_mutex_init(&pmutex, NULL);
    pthread_cond_init(&pcond, NULL);

    return 0;
}

void Rpmsg::closeRpmsg(void)
{
    if (fd_rpmsg > 0){
        close(fd_rpmsg);
        fd_rpmsg = -1;
    }
    pthread_cond_destroy(&pcond);
	pthread_mutex_destroy(&pmutex);
}

int Rpmsg::Rpmsg_SendDate(const char *send_buf, int data_len)
{
    int ret = 0;
    fd_set fs_write;
    FD_SET(fd_rpmsg, &fs_write); 

    if (fd_rpmsg < 0)
    {
        return -1;
    }
//    printf("send_buf = %s", send_buf);
    ret = write(fd_rpmsg, send_buf, data_len);
    if (ret != data_len)
    {
        tcflush(fd_rpmsg,TCOFLUSH);
    }
    
    return ret;
}

int Rpmsg::Rpmsg_RecvData(void)
{
    int ret = 0;
    fd_set fs_read;

    FD_ZERO(&fs_read);  
    FD_SET(fd_rpmsg, &fs_read); 

    if (fd_rpmsg < 0)
    {
        return -1;
    }
    // printf("%s %d rcv_buf\n", __func__, __LINE__ );
    ret = select(fd_rpmsg + 1, &fs_read, NULL, NULL, NULL);
    if (ret > 0)
    {
        memset(rcv_buf, 0 , sizeof(rcv_buf));
        ret = read(fd_rpmsg, rcv_buf, sizeof(rcv_buf));
        printf("%s %d rcv_buf = %s\n", __func__, __LINE__, rcv_buf);
    }
    printf("%s %d rcv_buf\n", __func__, __LINE__ );
    return ret;
}

int Rpmsg::Rpmsg_keyData(void)
{
    int ret = -1;
    while(1)
    {
        // ret = Rpmsg_RecvData();
        // printf("%s %d\n", __func__, __LINE__);
        if (strncmp(rcv_buf, COMMAND_KEY1, 5) == 0)
        {
            // printf("key : %s\n", COMMAND_KEY1);
            ret = 1;
            break;
        }
        else if (strncmp(rcv_buf, COMMAND_KEY0, 5) == 0)
        {
            // printf("key : %s\n", COMMAND_KEY0);
            ret = 0;
            break;
        }
        // pthread_mutex_lock(&pmutex);
        // pthread_cond_signal(&pcond);
        // pthread_mutex_unlock(&pmutex);
    }
    return ret;
}

int Rpmsg::Rpmsg_GpioData(void)
{
    int ret = 0;
    while(1)
    {
        // pthread_mutex_lock(&pmutex);
        // pthread_cond_wait(&pcond, &pmutex);
        // pthread_mutex_unlock(&pmutex);
        // ret = Rpmsg_RecvData();
        if  (strncmp(rcv_buf, COMMAND_PLC10, 5) == 0)
        {
            // printf("plc1 : %s\n", COMMAND_PLC10);
            ret = 0;
            break;
        }
        else if(strncmp(rcv_buf, COMMAND_PLC11, 5) == 0)
        {
            // printf("plc1 : %s\n", COMMAND_PLC11);
            ret = 1;
            break;
        }   
        if (strncmp(rcv_buf, COMMAND_PLC20, 5) == 0)
        {
            // printf("plc2 : %s\n", COMMAND_PLC20);
            ret = 2;
            break;
        }
        else if(strncmp(rcv_buf, COMMAND_PLC21, 5) == 0)
        {
            // printf("plc2 : %s\n", COMMAND_PLC21);
            ret = 3;
            break;
        }
        
    }
    return ret;
}

int Rpmsg::setLightAim(int status)
{
    int ret = 0;

    char command[10] = { 0 };
    sprintf(command, "%s10%d0", light_head,status);
    ret = Rpmsg_SendDate(command, strlen(command));

    return ret >= 0 ? 0 : -1;
}

int Rpmsg::setLightBright(int num, int value)
{
    int ret = 0;
    char command[20] = { 0 };
    m_light[num].num = num;
    m_light[num].value = value;
    sprintf(command, "%s%d%d%d%d", light_head, num, m_light[num].state, value/10, value%10);
//    printf("command = %s\n",command);
    ret = Rpmsg_SendDate(command, strlen(command));
    
    return ret >= 0 ? 0 : -1;
}


int Rpmsg::setLightEnable(int num, int state, int isLensFocus)
{
    int ret = 0;
    
    char command[20] = { 0 };
    if (isLensFocus)
    {
        if (state == 1)
            state = m_light[num].state;
    }
    else
    {
        m_light[num].state = state;
    }
    sprintf(command, "%s%d%d%d%d", light_head, num, state, m_light[num].value/10, m_light[num].value%10);

    ret = Rpmsg_SendDate(command, strlen(command));
    
    return ret >= 0 ? 0 : -1;
}

int Rpmsg::setkeyStatus(int num, bool status)
{
    int ret = 0;

    char command[10] = { 0 };
    sprintf(command, "%s%d%d0", key_head, num, status);
    ret = Rpmsg_SendDate(command, strlen(command));

    return ret >= 0 ? 0 : -1;
}

int Rpmsg::getDeviceStatus(void)
{
    if (fd_rpmsg < 0)
    {
        return -1;
    }
    return 0;
}


void  Rpmsg::testtime(void)
{
    char sendbuf[20] = "testtesttest";
    char recbuf[20] = { 0 };
    struct timeval tBegin, tEnd;
    gettimeofday(&tBegin, NULL);
    Rpmsg_SendDate(sendbuf, strlen(sendbuf));
    // Rpmsg_RecvData(recbuf, 20, -1);
    gettimeofday(&tEnd, NULL);
    printf("diff time = %d\n", get_diff_time(tBegin, tEnd));
}


int Rpmsg::camera_power(bool state)
{
    int ret = 0;
    char recbuf[20] = { 0 };

    if (state)
    {
        ret = Rpmsg_SendDate(camera_open, strlen(camera_open));
        // ret = Rpmsg_RecvData(recbuf, 20, -1);
    }
    else
    {
        ret = Rpmsg_SendDate(camera_close, strlen(camera_close));
        // ret = Rpmsg_RecvData(recbuf, 20, -1);
    }
    return ret >= 0 ? 0: -1; 
}

int Rpmsg::setLedStatus(int num, int value)
{
    int ret = 0;

    char command[10] = { 0 };
    sprintf(command, "%s%d%d", led_head, num, value);
    ret = Rpmsg_SendDate(command, strlen(command));

    return ret >= 0 ? 0 : -1; 
}
    
int Rpmsg::setPlcStatus(int num, int value)
{
    int ret = 0;

    char command[10] = { 0 };
    sprintf(command, "%s%d%d", plc_head, num, value);
    ret = Rpmsg_SendDate(command, strlen(command));

    return ret >= 0 ? 0 : -1; 
}
