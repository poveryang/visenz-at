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
INCLUDE_PATH += -I$(NVT_HDAL_DIR)/vendor/include -I$(NVT_AI_PATH)/include -I$(NVT_AI_KDRV_PATH)/include -I$(NVT_AI_KFLOW_PATH)/include
ifeq ("$(wildcard $(OWN_NVT_AI_PATH))", "")
INCLUDE_PATH += -I$(NVT_HDAL_DIR)/vendor/ai/source/prebuilt/include
else
INCLUDE_PATH += -I$(OWN_NVT_AI_PATH)/source/prebuilt/include 
endif
INCLUDE_PATH += -I$(NVT_HDAL_DIR)/vendor/cv/include -I$(NVT_HDAL_DIR)/vendor/isp/include
INCLUDE_PATH += -I$(NVT_HDAL_DIR)/vendor/dsp/include

#COPS = $(INCLUDE_PATH) -D ARM_ASM -O3 -Wall -Werror -finline-functions -msoft-float -fPIC -Wno-aggressive-loop-optimizations
COPS = $(INCLUDE_PATH) -D ARM_ASM -O3 -Wall -Werror -finline-functions -fPIC -Wno-aggressive-loop-optimizations

CC=$(CROSS_COMPILE)gcc
AS=$(CROSS_COMPILE)as
AR=$(CROSS_COMPILE)ar

CSRC += \
	custnn/net_cust_layer.c \
	custnn/custnn_cpu_util.c \
	custnn/custnn_dsp_pool.c \
	custnn/custnn_cpu_eltwise.c \
	custnn/custnn_cpu_concat.c \
	custnn/custnn_cpu_slice.c
CPPSRC += \
	custnn/custnn_cpu_pool.cpp

HDAL_STATIC_LIB = libvendor_ai_pub.a
HDAL_DYNAMIC_LIB = libvendor_ai_pub.so

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

install:
	cp *.so *.a $(OUTPUT_DIR)

clean :
	/bin/rm -f *.o
	/bin/rm -f */*.o
	/bin/rm -f $(HDAL_STATIC_LIB) $(HDAL_DYNAMIC_LIB)
endif

###############################################################################
# rtos Makefile                                                               #
###############################################################################
else ifeq ($(NVT_PRJCFG_CFG),rtos)

MODULE_NAME = libvendor_ai_pub
C_CFLAGS = $(PLATFORM_CFLAGS) $(INCLUDE_PATH)
C_CXXFLAGS = $(PLATFORM_CXXFLAGS) $(INCLUDE_PATH)
LIB_NAME = $(MODULE_NAME).a

SRC += \
	custnn/net_cust_layer.c \
	custnn/custnn_cpu_eltwise.c \
	custnn/custnn_cpu_util.c \
	custnn/custnn_dsp_pool.c \
	custnn/custnn_cpu_concat.c \
	custnn/custnn_cpu_slice.c
CPPSRC = \
	custnn/custnn_cpu_pool.cpp

OBJ = $(SRC:.c=.o) $(CPPSRC:.cpp=.o) 

ENV_LIBC=rtos 

all: $(LIB_NAME)

$(LIB_NAME): $(OBJ)
	@echo Creating $@...
	@$(AR) rcsD $@ $(OBJ)
	@$(BUILD_DIR)/nvt-tools/nvt-ld-op --arc-sha1 $@

%.o: %.c
	@echo Compiling $<
	@$(CC) $(C_CFLAGS) -c $(COPS) $< -o $@

%.o: %.cpp
	@echo Compiling $<
	@$(CXX) $(C_CXXFLAGS) -c $(COPS) $< -o $@	
	
clean:
	@rm -f $(LIB_NAME) $(OBJ) $(LIB_NAME).sym *.o *.a *.so*
	@rm -rf $(OUTPUT_DIR)

install:
	@cp *.a $(OUTPUT_DIR)
#	@cp $(PREBUILD_LIB)/*.a $(OUTPUT_DIR)
#	@cp -avf prebuilt/$(ENV_LIBC)/*.a $(OUTPUT_DIR)


endif
