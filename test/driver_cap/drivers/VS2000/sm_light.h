#ifndef __SMLIGHT_H
#define __SMLIGHT_H

#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>

typedef enum 
{
	HW_LIGHT_16 = 0,
	HW_LIGHT_34,
}HW_LIGHT_VERSION;


class SM_LIGHT
{
public:
    static SM_LIGHT &getInstance()
    {
        static SM_LIGHT instance;
        return instance;
    }

public:
    SM_LIGHT()
    {
        init_light();
    };
    ~SM_LIGHT()
    {
        close_light();
    };
    SM_LIGHT(const SM_LIGHT &) = delete;
    SM_LIGHT &operator=(const SM_LIGHT &) = delete;

    /**
     * @brief init_light:init uart3, Communicate with MCU 
     * @return 0, success, others: fail
     */
    int init_light(void);

    /**
     * @brief close_light:close uart
     * @return 
     */
    void close_light(void);

    /**
     * @brief init_light:init uart3, Communicate with MCU 
     * @return 0, success, others: fail
     */
    int light_config(int nSpeed, int nBits, int nParity, int nStop);

    /**
     * @brief init_light:init uart3, Communicate with MCU 
     * @return 0, success, others: fail
     */
    int Light_Recv(char *rcv_buf, int data_len, int timeout);

    /**
     * @brief init_light:init uart3, Communicate with MCU 
     * @return 0, success, others: fail
     */
    int Lihgt_Send(char *send_buf, int data_len);

    /**
     * @brief init_light:init uart3, Communicate with MCU 
     * @return 0, success, others: fail
     */
    int setLightAim(int status);

    /**
     * @brief setLightStatus:init uart3, Communicate with MCU 
     * @return 0, success, others: fail
     */
    int setLightStatus(int num, int value);

    int getLightHWVersion(void);

    int getLightHWColor(void);

    char *getMcuVersion(char *buf);

private:
    const char * const light_name = "/dev/ttymxc2";
    const char * const light_head = "light";        //light head   
    const char * const light_hwversion = "version"; //hwversion 
    const char * const mcuversion = "mcuversion";   //mcuversion  
    int fd_light = -1;
};

#endif

