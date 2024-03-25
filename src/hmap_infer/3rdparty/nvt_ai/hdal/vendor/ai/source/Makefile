current_mk_abspathname := $(abspath $(lastword $(MAKEFILE_LIST)))
current_mk_absdir := $(dir $(current_mk_abspathname))../../ai_sdk_env.mk
ifneq ("$(wildcard $(current_mk_absdir))", "")
include $(current_mk_absdir)
endif
CUR_PATH := $(shell pwd)
NVT_KDRV_PATH=$(NVT_HDAL_DIR)/drivers/k_driver
NVT_KFLOW_PATH=$(NVT_HDAL_DIR)/drivers/k_flow
ifeq ("$(wildcard $(OWN_NVT_AI_PATH))", "")
NVT_AI_PATH=$(NVT_HDAL_DIR)/vendor/ai
NVT_AI_KDRV_PATH=$(NVT_AI_PATH)/drivers/k_driver
NVT_AI_KFLOW_PATH=$(NVT_AI_PATH)/drivers/k_flow
endif
OUTPUT_DIR = $(NVT_HDAL_DIR)/vendor/output

INCLUDE_PATH += -I$(NVT_VOS_DIR)/drivers/include
INCLUDE_PATH += -I$(NVT_HDAL_DIR)/include
INCLUDE_PATH += -I$(NVT_KDRV_PATH)/include -I$(NVT_KDRV_PATH)/source/include/
INCLUDE_PATH += -I$(NVT_HDAL_DIR)/vendor/include -I$(NVT_AI_PATH)/include -I$(NVT_AI_KDRV_PATH)/include -I$(NVT_AI_KFLOW_PATH)/include -I$(NVT_KFLOW_PATH)/include
ifeq ("$(wildcard $(OWN_NVT_AI_PATH))", "")
INCLUDE_PATH += -I$(NVT_HDAL_DIR)/vendor/ai/source/prebuilt/include
else
INCLUDE_PATH += -I$(OWN_NVT_AI_PATH)/source/prebuilt/include 
endif
INCLUDE_PATH += -I$(NVT_HDAL_DIR)/vendor/cv/include -I$(NVT_HDAL_DIR)/vendor/isp/include
INCLUDE_PATH += -I$(NVT_HDAL_DIR)/vendor/dsp/include -I$(NVT_HDAL_DIR)/vendor/media/include

#COPS = $(INCLUDE_PATH) -D ARM_ASM -O3 -Wall -Werror -finline-functions -msoft-float -fPIC -Wno-aggressive-loop-optimizations
COPS = $(INCLUDE_PATH) -D ARM_ASM -O3 -Wall -Werror -finline-functions -fPIC -Wno-aggressive-loop-optimizations -D__LINUX_USER__ -D__LINUX -mfpu=neon-fp16

ifeq ("$(wildcard $(OWN_NVT_AI_PATH))", "")
uclibc=$(shell echo $(CROSS_COMPILE)|grep uclib)
ifeq ($(uclibc),)
    PREBUILD_LIB=$(NVT_HDAL_DIR)/vendor/ai/source/prebuilt/lib/glibc
else
    PREBUILD_LIB=$(NVT_HDAL_DIR)/vendor/ai/source/prebuilt/lib/uclibc
endif
endif

CC=$(CROSS_COMPILE)gcc
AS=$(CROSS_COMPILE)as
AR=$(CROSS_COMPILE)ar

CSRC += \
	net_gen_sample/net_gen_sample.c \
	net_flow_user_sample/net_flow_user_sample.c \
	net_flow_user_sample/net_nvtnn_layer.c \
	net_post_sample/net_post_sample_softmax.c \
	net_post_sample/net_post_sample_accuracy.c \
	net_post_sample/net_post_sample_util.c \
	vendor_ai/vendor_ai.c \
	vendor_preproc/vendor_preproc.c

	
HDAL_STATIC_LIB = libvendor_ai.a
HDAL_DYNAMIC_LIB = libvendor_ai.so

COBJS = ${CSRC:%.c=%.o} ${CPPSRC:%.cpp=%.o}
#LOPTS = -g

TARGETS := $(HDAL_STATIC_LIB) $(HDAL_DYNAMIC_LIB)

.PHONY: $(TARGETS)
###############################################################################
# Linux Makefile                                                              #
###############################################################################
ifeq ($(NVT_PRJCFG_CFG),Linux)
ifeq ("$(wildcard *.c */*.c)","")
all:
	@echo "nothing to be done for '$(OUTPUT_NAME)'"
clean:
	@echo "nothing to be done for '$(OUTPUT_NAME)'"
codesize:
	@echo "nothing to be done for '$(OUTPUT_NAME)'"		
else
all: $(TARGETS)

$(HDAL_STATIC_LIB) : $(COBJS) $(SOBJS) 
	$(AR) -r $@ $(COBJS)
#	mkdir -p ./lib
#	cp $@ ./lib/

$(HDAL_DYNAMIC_LIB) : $(COBJS) $(SOBJS) 
	$(CC) -shared -s -o $@ $(COBJS)
#	mkdir -p ../lib
#	cp $@ ./lib/

%.o: %.c
	$(CC) -c $(COPS) $(CFLAGS) -I. -o $@ $<
	
%.o: %.cpp
	$(CC) -c $(COPS) $(CFLAGS) -I. -o $@ $<

clean :
	/bin/rm -f *.o
	/bin/rm -f */*.o
	/bin/rm -f $(HDAL_STATIC_LIB) $(HDAL_DYNAMIC_LIB)
endif

install:
#	cp *.so *.a $(ROOTFS_DIR)/rootfs/lib
	cp *.so *.a $(OUTPUT_DIR)
#	cp $(PREBUILD_LIB)/*.so $(PREBUILD_LIB)/*.a $(ROOTFS_DIR)/rootfs/lib
	cp $(PREBUILD_LIB)/*.so $(PREBUILD_LIB)/*.a $(OUTPUT_DIR)
###############################################################################
# rtos Makefile                                                               #
###############################################################################
else ifeq ($(NVT_PRJCFG_CFG),rtos)

MODULE_NAME = libvendor_ai
C_CFLAGS = $(PLATFORM_CFLAGS) $(INCLUDE_PATH)
LIB_NAME = $(MODULE_NAME).a

PREBUILD_LIB=$(NVT_HDAL_DIR)/vendor/ai/source/prebuilt/lib


SRC = \
  net_flow_user_sample/net_flow_user_sample.c \
  net_gen_sample/net_gen_sample.c \
  net_post_sample/net_post_sample_util.c \
  net_post_sample/net_post_sample_accuracy.c \
  vendor_ai/vendor_ai.c \
  net_post_sample/net_post_sample_softmax.c \
  net_flow_user_sample/net_nvtnn_layer.c \
  vendor_preproc/vendor_preproc.c \

OBJ = $(SRC:.c=.o)


all: $(LIB_NAME)

$(LIB_NAME): $(OBJ)
	@echo Creating $@...
	@$(AR) rcsD $@ $(OBJ)
	@$(BUILD_DIR)/nvt-tools/nvt-ld-op --arc-sha1 $@

%.o: %.c
	@echo Compiling $<
	@$(CC) $(C_CFLAGS) -c $< -o $@
	
clean:
	@rm -f $(LIB_NAME) $(OBJ) $(LIB_NAME).sym *.o *.a *.so*
	@rm -rf $(OUTPUT_DIR)

install:
	@mkdir -p $(OUTPUT_DIR)
	@cp -avf $(LIB_NAME) $(OUTPUT_DIR)
	@cp -avf prebuilt/lib/rtos/*.a $(OUTPUT_DIR)

endif