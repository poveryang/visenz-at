/**
	@brief Header file of IO control definition of vendor net flow sample.

	@file net_flow_sample_ioctl.h

	@ingroup net_flow_sample

	@note Nothing.

	Copyright Novatek Microelectronics Corp. 2018.  All rights reserved.
*/
#ifndef _NET_FLOW_SAMPLE_IOCTL_H_
#define _NET_FLOW_SAMPLE_IOCTL_H_

#include <linux/cdev.h>
#include <kwrap/dev.h>
#define DEV_UPDATE 1

#if DEV_UPDATE
#define MODULE_NAME "netflowsdev"
#define MODULE_MINOR_COUNT 1
#endif
/********************************************************************
	TYPE DEFINITION
********************************************************************/
/**
	Net flow device
*/
#if DEV_UPDATE
typedef struct netflow_drv_info {
	struct class *pmodule_class;
	struct device *pdevice[MODULE_MINOR_COUNT];
	//struct resource* presource[MODULE_REG_NUM];
	struct cdev cdev;
	dev_t dev_id;
} NETFLOW_DRV_INFO, *PNETFLOW_DRV_INFO;
#endif
typedef struct {
	char                name[16];       ///< device name
	spinlock_t          ioctl_lock;     ///< device ioctl locker
	struct miscdevice   miscdev;        ///< device node for ioctl access
	//uint                mem_type;       ///< 0: nocache 1: cache
	//uint                mem_offset;
	//uint                mem_limitsz;
} vendor_ais_flow_dev_t;

/**
	Memory buffer
*/
typedef struct {
	int addr;                           ///< memory buffer starting address
	int size;                           ///< memory buffer size
} vendor_ais_mem_t;

/**
	Memory information
*/
typedef struct {
	int ddr_id;
	int bufsize;
	int type;
	void *va;                           ///< need to free
	dma_addr_t pa;
} vendor_ais_ddr_t;

#endif  /* _NET_FLOW_SAMPLE_IOCTL_H_ */
