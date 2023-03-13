#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include "sm_light.h"
#include "sys/time.h"


int SM_LIGHT::init_light(void)
{
    int ret = 0;
    if (fd_light > 0)
        return -1;

    fd_light = open(light_name, O_RDWR | O_NOCTTY );   //O_RDWR | O_NOCTTY
    if (fd_light < 0)
    {
        perror(light_name);
        return -1;
    } 

    ret = light_config(115200, 8, 'N', 1);
    return ret;
}

void SM_LIGHT::close_light(void)
{
    if (fd_light > 0){
        close(fd_light);
        fd_light = -1;
    }
}

int SM_LIGHT::light_config(int nSpeed, int nBits, int nParity, int nStop)
{
    if (fd_light < 0)
    {
        return -1;
    }
    struct termios newtio, oldtio;

    if (tcgetattr(fd_light, &oldtio) != 0)
    {
        perror("SetupSerial 1");
        return -1;
    }
    bzero(&newtio, sizeof(newtio));        
    newtio.c_cflag |= CLOCAL | CREAD;   
    newtio.c_cflag &= ~CSIZE;  

    switch (nBits)
    {
        case 7:
            newtio.c_cflag |= CS7;
        break;
        case 8:
            newtio.c_cflag |= CS8;
        break;
        default:
            fprintf(stderr, "Unsupported data size\n");
            return -1;
    }

    switch (nParity)
    {
        case 'o':
        case 'O':                     //奇校验
            newtio.c_cflag |= PARENB;
            newtio.c_cflag |= PARODD;
            newtio.c_iflag |= (INPCK | ISTRIP);
            break;
        case 'e':
        case 'E':                     //偶校验
            newtio.c_iflag |= (INPCK | ISTRIP);
            newtio.c_cflag |= PARENB;
            newtio.c_cflag &= ~PARODD;
            break;
        case 'n':
        case 'N':                     //无校验
            newtio.c_cflag &= ~PARENB;
            break;
        default:
            fprintf(stderr, "Unsupported parity\n");
            return -1;
    }
	newtio.c_iflag &= ~(IXON | IXOFF | IXANY);

    switch (nStop)
    {
        case 1:
            newtio.c_cflag &= ~CSTOPB;
        break;
        case 2:
            newtio.c_cflag |= CSTOPB;
        break;
        default:
            fprintf(stderr,"Unsupported stop bits\n");
            return -1;
    }
    printf("c_iflag=0x%x, c_oflag=0x%x, c_lflag=0x%x\n", newtio.c_iflag , newtio.c_oflag, newtio.c_lflag);
    
    switch (nSpeed)
    {
        case 2400:
            cfsetispeed(&newtio, B2400);
            cfsetospeed(&newtio, B2400);
            break;
        case 4800:
            cfsetispeed(&newtio, B4800);
            cfsetospeed(&newtio, B4800);
            break;
        case 9600:
            cfsetispeed(&newtio, B9600);
            cfsetospeed(&newtio, B9600);
            break;
        case 19200:
            cfsetispeed(&newtio, B19200);
            cfsetospeed(&newtio, B19200);
            break;
        case 38400:
            cfsetispeed(&newtio, B38400);
            cfsetospeed(&newtio, B38400);
            break;
        case 57600:
            cfsetispeed(&newtio, B57600);
            cfsetospeed(&newtio, B57600);
            break;
        case 115200:
            cfsetispeed(&newtio, B115200);
            cfsetospeed(&newtio, B115200);
            break;
        case 230400:
            cfsetispeed(&newtio, B230400);
            cfsetospeed(&newtio, B230400);
            break;
        default:
            // printf("\tSorry, Unsupported baud rate, set default 9600!\n\n");
            cfsetispeed(&newtio, B9600);
            cfsetospeed(&newtio, B9600);
            break;
    }

    newtio.c_cc[VTIME] = 1;    
    newtio.c_cc[VMIN] = 1;        

    tcflush(fd_light,TCIFLUSH);         
    if (tcsetattr(fd_light, TCSANOW, &newtio) != 0)    
    {
        perror("SetupSerial 3");
        return -1;
    }
    printf("Serial set done!\n");
    return 0;
}


int SM_LIGHT::Light_Recv(char *rcv_buf, int data_len, int timeout)
{
    if (fd_light < 0)
    {
        return -1;
    }
    int len, fs_sel;
    fd_set fs_read;
    struct timeval time;

    FD_ZERO(&fs_read);       
    FD_SET(fd_light, &fs_read);   

    time.tv_sec = timeout / 1000;             
    time.tv_usec = timeout % 1000 * 1000;    
    fs_sel = select(fd_light + 1, &fs_read, NULL, NULL,timeout == -1 ? NULL : &time);

    if(fs_sel > 0)
    {
        len = read(fd_light, rcv_buf, data_len);
        printf("%s %d rcv_buf = %s, len = %d\n", __func__, __LINE__, rcv_buf, len);
        return len;
    }
    else
    {
        return -1;
    }
}

int SM_LIGHT::Lihgt_Send(char *send_buf, int data_len)
{
    if (fd_light < 0)
    {
        return -1;
    }
    ssize_t ret = 0;
    ret = write(fd_light, send_buf, data_len);
    if (ret == data_len)
    {
        printf("send data is %s\n", send_buf);
        return ret;
    }
    else
    {
        printf("write device error\n");
        tcflush(fd_light,TCOFLUSH);
        return -1;
    }
}

int SM_LIGHT::setLightAim(int status)
{
    int ret = 0;

    char command[20] = { "\0" };
    sprintf(command, "%s7%d", light_head, status);
    Lihgt_Send(command, strlen(command)+1);
    memset(command, 0, sizeof(command));
    Light_Recv(command, strlen("O"), 10);
    ret = strcmp(command, "O");
    return ret == 0 ? 0 : -1;
}

int SM_LIGHT::setLightStatus(int num, int value)
{
    int ret = 0;
    char command[20] = { "\0" };

    for (int i = 0 ; i < 5; i++)
    {
        sprintf(command, "%s%d%d", light_head, num, value > 0 ? 1 : 0);
        Lihgt_Send(command, strlen(command)+1);
        memset(command, 0, sizeof(command));
        Light_Recv(command, strlen("O"), 10);
        ret = strcmp(command, "O");
        if (ret == 0)
            break;
    } 

    return ret == 0 ? 0 : -1;
}

int SM_LIGHT::getLightHWVersion(void)
{
    int ret = 0;
    char command[20] = { "\0" };

    sprintf(command, "%s%d", light_hwversion, 1);
    Lihgt_Send(command, strlen(command)+1);
    memset(command, 0, sizeof(command));
    Light_Recv(command, 20, 10);
    char c = command[0];
    if (c == '0')
    {
        ret = 4;
    }
    else if (c == '1')
    {
        ret = 6;
    }
    return ret;
}

int SM_LIGHT::getLightHWColor(void)
{
    int ret = 0;
    char command[20] = { "\0" };

    sprintf(command, "%s%d", light_hwversion, 2);
    Lihgt_Send(command, strlen(command)+1);
    memset(command, 0, sizeof(command));
    Light_Recv(command, 20, 10);
    ret = command[0] - '0';
    return ret;
}

char *SM_LIGHT::getMcuVersion(char *buf)
{
    char command[20] = { "\0" };

    sprintf(command, "%s", mcuversion);
    Lihgt_Send(command, strlen(command)+1);
    Light_Recv(buf, 15, 10);

    return buf;
}
