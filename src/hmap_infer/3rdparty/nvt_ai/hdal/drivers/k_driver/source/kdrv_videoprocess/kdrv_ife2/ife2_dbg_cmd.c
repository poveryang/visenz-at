
#if defined (__UITRON) || defined (__ECOS)

#inc <malloc.h>
#inc <stdio.h>
#inc <string.h>
#inc <stdlib.h>

#inc "Type.h"
#inc "kernel.h"

#elif defined (__FREERTOS )

#inc <stdio.h>
#inc <string.h>
#inc <stdlib.h>


#inc <kwrap/file.h>

#inc <malloc.h>
#inc "kwrap/type.h"
#inc "kwrap/platform.h"


#elif defined (__KERNEL__) || defined (__LINUX)
#inc <linux/string.h>
#inc <linux/printk.h>
#inc <linux/kernel.h>
#inc <linux/slab.h>
#inc <linux/fs.h>
#inc <linux/uaccess.h>
#inc <comm/nvtmem.h>

#inc "kwrap/type.h"
#inc "kwrap/platform.h"
#inc <kwrap/file.h>

#inc "mach/rcw_macro.h"
#inc "mach/nvt-io.h"
#inc "kwrap/type.h"
#inc <plat-na51055/nvt-sramctl.h>
//#inc <frammap/frammap_if.h>
#inc <mach/fmem.h>

#inc "ife2_drv.h"

#endif

#include "ife2_dbg.h"

#include "ife2_dbg_cmd.h"
#include "ife2_kit.h"
#include "ife2_platform.h"
#include <kwrap/cmdsys.h>
#include "kwrap/sxcmd.h"


#if (__IFE2_DBG_CMD__ == 1)
static BOOL cmd_ife2_dump_info(unsigned char argc, char **argv)
{
	//exam_msg("kdrv_ife2 test\r\n");

	DBG_DUMP("kdrv_ife2 test\r\n");
	nvt_ife2_api_dump(argc, argv);
	return TRUE;
}

static BOOL cmd_ife2_dump_test(unsigned char argc, char **argv)
{
	nvt_kdrv_ife2_test(argc, argv);
	return TRUE;
}
#endif

#if !defined (CONFIG_NVT_SMALL_HDAL)
static BOOL cmd_ife2_hw_power_saving(unsigned char argc, char **argv)
{
	nvt_ife2_hw_power_saving(argc, argv);
	return TRUE;
}

static BOOL cmd_ife2_fw_power_saving(unsigned char argc, char **argv)
{
	nvt_ife2_fw_power_saving(argc, argv);
	return TRUE;
}
#endif



static SXCMD_BEGIN(ife2_cmd_tbl, "kdrv_ife2")
#if (__IFE2_DBG_CMD__ == 1)
SXCMD_ITEM("info",        cmd_ife2_dump_info,         "dump kdrv_ife2 info")
SXCMD_ITEM("test %s",     cmd_ife2_dump_test,         "test kdrv_ife2 ")
#endif
#if !defined (CONFIG_NVT_SMALL_HDAL)
SXCMD_ITEM("ife2_hw_pwsv %s",  cmd_ife2_hw_power_saving,	 "Enable/Disable HW power saving")
SXCMD_ITEM("ife2_fw_pwsv %s",  cmd_ife2_fw_power_saving,	 "Enable/Disable FW power saving")
#endif
SXCMD_END()

int ife2_cmd_showhelp(int (*dump)(const char *fmt, ...))
{
	UINT32 cmd_num = SXCMD_NUM(ife2_cmd_tbl);
	UINT32 loop = 1;

	dump("---------------------------------------------------------------------\r\n");
	dump("  %s\n", "ife2");
	dump("---------------------------------------------------------------------\r\n");

	for (loop = 1 ; loop <= cmd_num ; loop++) {
		dump("%15s : %s\r\n", ife2_cmd_tbl[loop].p_name, ife2_cmd_tbl[loop].p_desc);
	}
	return 0;
}


#if defined (__KERNEL__) || defined(__LINUX)
int ife2_cmd_execute(unsigned char argc, char **argv)
{
	UINT32 cmd_num = SXCMD_NUM(ife2_cmd_tbl);
	UINT32 loop;
	int    ret;

	if (argc < 1) {
		return -1;
	}
	if (strncmp(argv[0], "?", 2) == 0) {
		ife2_cmd_showhelp(vk_printk);
		return 0;
	}
	for (loop = 1 ; loop <= cmd_num ; loop++) {
		if (strncmp(argv[0], ife2_cmd_tbl[loop].p_name, strlen(argv[0])) == 0) {
			ret = ife2_cmd_tbl[loop].p_func(argc, argv);
			return ret;
		}
	}
	if (loop > cmd_num) {
		DBG_ERR("Invalid CMD !!\r\n");
		ife2_cmd_showhelp(vk_printk);
		return -1;
	}
	return 0;
}
#else
MAINFUNC_ENTRY(kdrv_ife2, argc, argv)
{
	UINT32 cmd_num = SXCMD_NUM(ife2_cmd_tbl);
	UINT32 loop;
	int    ret;

	if (argc < 2) {
		return -1;
	}
	if (strncmp(argv[1], "?", 2) == 0) {
		ife2_cmd_showhelp(vk_printk);
		return 0;
	}
	for (loop = 1 ; loop <= cmd_num ; loop++) {
		if (strncmp(argv[1], ife2_cmd_tbl[loop].p_name, strlen(argv[1])) == 0) {
			ret = ife2_cmd_tbl[loop].p_func(argc - 2, &argv[2]);
			return ret;
		}
	}
	if (loop > cmd_num) {
		DBG_ERR("Invalid CMD !!\r\n");
		ife2_cmd_showhelp(vk_printk);
		return -1;
	}
	return 0;
}
#endif




