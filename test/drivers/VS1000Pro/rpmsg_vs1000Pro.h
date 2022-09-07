#ifndef __RPMSG_H
#define __RPMSG_H

#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <pthread.h>

#define LIGHT_NUM				4
struct light_info
{
    int num;
    int value;
    int state;
};

class Rpmsg
{
public:
    static Rpmsg &getInstance()
    {
        static Rpmsg instance;
        return instance;
    }

public:
    Rpmsg()
    {
        initRpmsg();
    };
    ~Rpmsg()
    {
        closeRpmsg();
    };
    Rpmsg(const Rpmsg &) = delete;
    Rpmsg &operator=(const Rpmsg &) = delete;

    /**
     * @brief initRpmsg:init rpmsg,Communicate with M4 
     * @return 0, success, others: fail
     */
    int initRpmsg(void);

    /**
     * @brief initRpmsg:close rpmsg
     * @return 0, success, others: fail
     */
    void closeRpmsg(void);

    /**
     * @brief getDeviceStatus:get rpmsg status
     * @return 0, on state, others: off state
     */
    int getDeviceStatus(void);

    /**
     * @brief Rpmsg_SendDate, send date to M4
     * @param send_buf
     * @param data_len:
     * @return 0, success, others: fail
     */
    int Rpmsg_SendDate(const char *send_buf, int data_len);

    /**
     * @brief Rpmsg_RecvData, recvie date from M7
     * @param rcv_buf
     * @param data_len
     * @param timeout
     * @return 0, success, others: fail
     */
    int Rpmsg_RecvData(void);

    /**
     * @brief setLightAim, set aim light
     * @return 0, success, others: fail
     */
    int setLightAim(int status);

    /**
     * @brief setLightBright, set light bright
     * @param num
     * @param value:
     * @return 0, success, others: fail
     */
    int setLightBright(int num, int value);

    /**
     * @brief setLightEnable, set light state
     * @param num
     * @param state:
     * @return 0, success, others: fail
     */
    int setLightEnable(int num, int state, int isLensFocus);

    /**
     * @brief setkeyStatus, set key enable 
     * @param num
     * @param status
     * @return 0, success, others: fail
     */
    int setkeyStatus(int num, bool status);

    int get_diff_time(struct timeval timeStart, struct timeval timeEnd);

    void testtime(void);

    /**
     * @brief setkeyStatus, set key enable 
     * @param state : 1 on, 0 off
     * @return 0, success, others: fail
     */
    int camera_power(bool state);

    /**
     * @brief Rpmsg_GpioData, set key enable 
     * @return -1 : fail
     * @return channel 1, rising edge: 0 ; falling edge: 1
     * @return channel 2, rising edge: 2 ; falling edge: 3
     */
    int Rpmsg_GpioData(void);

    /**
     * @brief Rpmsg_GpioData, set key enable 
     * @return 0 : press, 1 : up, -1 :fail
     */
    int Rpmsg_keyData(void);

    /**
     * @brief setLedStatus, set led state
     * @param num
     * @param state:
     * @return 0, success, others: fail
     */
    int setLedStatus(int num, int value);

    /**
     * @brief setPlcStatus, set plc state
     * @param num
     * @param state:
     * @return 0, success, others: fail
     */
    int setPlcStatus(int num, int value);
    
private:
    const char *rpmsg_name = "/dev/ttyRPMSG30";     
    const char *light_head = "light";       //light head   
    const char *key_head = "key";           //key head
    const char *led_head = "led";           //led head
    const char *plc_head = "plc";           //plc head   
    const char *camera_open = "camera1";
    const char *camera_close = "camera0";
    const char* const COMMAND_KEY0 = "key10";
    const char* const COMMAND_KEY1 = "key11";
    const char* const COMMAND_PLC10 = "plc10";
    const char* const COMMAND_PLC11 = "plc11";
    const char* const COMMAND_PLC20 = "plc20";
    const char* const COMMAND_PLC21 = "plc21";

    char rcv_buf[128] = { 0 };
    int fd_rpmsg = -1;
    struct timeval timeStart, timeEnd;
    struct light_info m_light[LIGHT_NUM];
    pthread_mutex_t pmutex;
    pthread_cond_t pcond;

};

#endif

