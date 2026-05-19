#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "mem_manager.h"

struct sm_mem_dev *sm_mem_dev_create(int buf_num, int size)
{
    int            fd;
    char  attr[1024];
    unsigned long  debug_vma = 0;
    unsigned long  sync_mode = 1;
	unsigned int  sync_direction = 2;
	unsigned int  sync_for_cpu = 1;
	unsigned int page_size;
	udma_buf *mem = NULL;
    char tmp[64];
    int i;

	struct sm_mem_dev *mem_dev;

	mem_dev = (struct sm_mem_dev *)malloc(sizeof(struct sm_mem_dev));
	if(!mem_dev){
		goto ERROR;
	}
	mem_dev->mem = (udma_buf *)malloc(sizeof(udma_buf) * buf_num);
	if(!mem_dev->mem){
		goto ERROR;
	}

	page_size = getpagesize();
	size = (size + page_size - 1) & ~(page_size - 1);
	
	for(i = 0; i < buf_num; i++){

		mem = &mem_dev->mem[i];
		
        memset(tmp, 0, sizeof tmp);
        sprintf(tmp, "/sys/class/u-dma-buf/udmabuf%d", i);

        if(access(tmp, F_OK) == F_OK){
                goto CREATE;
        }

        memset(tmp, 0, sizeof tmp);
        sprintf(tmp, "echo 'create udmabuf%d 0x%x' > /dev/u-dma-buf-mgr", i, size);
        system(tmp);
        printf("%s\n", tmp);
CREATE:
        memset(tmp, 0, sizeof tmp);
        sprintf(tmp, "/sys/class/u-dma-buf/udmabuf%d/phys_addr", i);
        if ((fd  = open(tmp, O_RDONLY)) != -1) {
          read(fd, attr, 1024);
          sscanf(attr, "%x", &mem->phys_addr);
          close(fd);
        }

        memset(tmp, 0, sizeof tmp);
        sprintf(tmp, "/sys/class/u-dma-buf/udmabuf%d/size", i);
        if ((fd  = open(tmp, O_RDONLY)) != -1) {
          read(fd, attr, 1024);
          sscanf(attr, "%d", &mem->size);
          close(fd);
        }
        memset(tmp, 0, sizeof tmp);
        sprintf(tmp, "/sys/class/u-dma-buf/udmabuf%d/sync_mode", i);
        if ((fd  = open(tmp, O_WRONLY)) != -1) {
          sprintf(attr, "%d", sync_mode);
          write(fd, attr, strlen(attr));
          close(fd);
        }
		
        memset(tmp, 0, sizeof tmp);
        sprintf(tmp, "/sys/class/u-dma-buf/udmabuf%d/sync_direction", i);
        if ((fd  = open(tmp, O_WRONLY)) != -1) {
          sprintf(attr, "%d", sync_direction);
          write(fd, attr, strlen(attr));
          close(fd);
        }
		
        memset(tmp, 0, sizeof tmp);
        sprintf(tmp, "/sys/class/u-dma-buf/udmabuf%d/sync_for_cpu", i);
        if ((fd  = open(tmp, O_WRONLY)) != -1) {
          sprintf(attr, "%d", sync_for_cpu);
          write(fd, attr, strlen(attr));
          close(fd);
        }

		memset(tmp, 0, sizeof tmp);
		sprintf(tmp, "/sys/class/u-dma-buf/udmabuf%d/debug_vma", i);
		if ((fd  = open(tmp, O_WRONLY)) != -1) {
		  sprintf(attr, "%d", debug_vma);
		  write(fd, attr, strlen(attr));
		  close(fd);
		}

//		printf("phys_addr[%d]=0x%x, size=%d\n", i, mem->phys_addr, mem->size);

		memset(tmp, 0, sizeof tmp);
		sprintf(tmp, "/dev/udmabuf%d", i);
	}
	return mem_dev;
ERROR:
	if(mem_dev){
		if(mem_dev->mem){
			free(mem_dev->mem);
		}
		free(mem_dev);	
	}
	return NULL;
}


void sm_mem_buf_sync(struct sm_mem_dev *mem_dev, int no)
{
	udma_buf *mem = NULL;
	int fd;

    char  attr[1024];
    char tmp[64];
	unsigned int  sync_direction = 2;
	unsigned int  sync_for_cpu = 1;

	if(!mem_dev){
		return;
	}
	
	mem = &mem_dev->mem[no];
	fd = mem->fd;

	memset(tmp, 0, sizeof tmp);
	sprintf(tmp, "/sys/class/u-dma-buf/udmabuf%d/sync_direction", no);
	if ((fd  = open(tmp, O_WRONLY)) != -1) {
	  sprintf(attr, "%d", sync_direction);
	  write(fd, attr, strlen(attr));
	  close(fd);
	}
	
	memset(tmp, 0, sizeof tmp);
	sprintf(tmp, "/sys/class/u-dma-buf/udmabuf%d/sync_for_cpu", no);
	if ((fd  = open(tmp, O_WRONLY)) != -1) {
	  sprintf(attr, "%d", sync_for_cpu);
	  write(fd, attr, strlen(attr));
	  close(fd);
	}
}

void sm_mem_free(struct sm_mem_dev *mem_dev, int buf_cnt)
{
	udma_buf *mem = NULL;
	int i;

	if(!mem_dev){
		return;
	}
	for(i = 0; i < buf_cnt; i++){
		mem = &mem_dev->mem[i];
		
		if(munmap((void *)mem->virt_uaddr, mem->size)){
			printf("sm_mem_free: munmap buffer%d failed\n", i);
		}
		close(mem->fd);
	}
}

int sm_mem_alloc(struct sm_mem_dev *mem_dev, int buf_cnt)
{
	udma_buf *mem = NULL;
	unsigned int page_size;

	char tmp[64];
	unsigned long  o_sync = 0;
	int i;

	if(!mem_dev){
		return -1;
	}
	
	for (i = 0; i < buf_cnt; i++) {
		
		mem = &mem_dev->mem[i];
		memset(tmp, 0, sizeof tmp);
		sprintf(tmp, "/dev/udmabuf%d", i);
		if ((mem->fd  = open(tmp, O_RDWR | o_sync)) == -1) {
		        goto err;
		}

		mem->virt_uaddr = (long unsigned int)mmap(NULL, mem->size, PROT_READ|PROT_WRITE, MAP_SHARED, mem->fd, 0);
//		printf("virt_uaddr[%d] =  0x%x\n", i,  mem->virt_uaddr);
	}
	return 0;
err:
	sm_mem_free(mem_dev, buf_cnt);
    return -1;
}


void sm_mem_destroy(struct sm_mem_dev *mem_dev)
{
	if(mem_dev){
		if(mem_dev->mem){
			free(mem_dev->mem);
		}
		free(mem_dev);
	}
}

unsigned long sm_mem_get_viraddr(struct sm_mem_dev *mem_dev, int no)
{
	return mem_dev->mem[no].virt_uaddr;
}


unsigned long sm_mem_get_phyaddr(struct sm_mem_dev *mem_dev, int no)
{
	return mem_dev->mem[no].phys_addr;
}


