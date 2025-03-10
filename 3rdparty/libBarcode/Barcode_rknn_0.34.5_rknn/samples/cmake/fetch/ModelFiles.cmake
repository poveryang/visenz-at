set(MODELFILES_LOCAL_PATH ${CMAKE_SOURCE_DIR}/3rdparty/)
set(DEEPLEARNING_PATH "https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/deeplearning")
set(X_DEEPLEARNING_PATH "https://devops-generic.pkg.coding.smoa.cloud/x-barcode/model")

# onnxruntime backend
set(ORT_DET_MODEL_NAME "release_models.zip")
# set(ORT_DECODE1D_NAME "bardecoder1d.smartmore")

# tengine backend
set(TENGINE_DET_MODEL_NAME "release_v6_hotfix_models.zip")
# set(TENGINE_DECODE1D_NAME "mstd_0811.tmfile")

# hailo backend
set(HAILO_DET_MODEL_NAME "v6_hotfix_hailo_model_0926.zip")

#novaic backend
set(NOVAIC_DET_MODEL_NAME "novaic_model.zip?version=20240729")

# rknn backend
set(RKNN_DET_MODEL_NAME "rknn_model.zip?version=20240805")

############################################################################################
if(NOT BUILD_WITH_DL)
    message(FATAL_ERROR "BUILD_WITH_DL must be on when you want to download models")
endif()

message("WITH_TENGINE is ${WITH_TENGINE}")

if(WITH_TENGINE)
    set(DET_MODEL_URL "${DEEPLEARNING_PATH}/tengine/${TENGINE_DET_MODEL_NAME}")
    set(DECODE_MODEL_URL "${DEEPLEARNING_PATH}/tengine/${TENGINE_DECODE1D_NAME}")
    set(DET_MODEL_NAME ${TENGINE_DET_MODEL_NAME})
    # set(DECODE1D_MODEL_NAME ${TENGINE_DECODE1D_NAME})
    set(DNN_BACKEND tengine_model)
elseif(WITH_ORT OR WITH_TRT)
    set(DET_MODEL_URL "${DEEPLEARNING_PATH}/win/${ORT_DET_MODEL_NAME}")
    set(DECODE_MODEL_URL "${DEEPLEARNING_PATH}/win/${ORT_DECODE1D_NAME}")
    set(DET_MODEL_NAME ${ORT_DET_MODEL_NAME})
    # set(DECODE1D_MODEL_NAME ${ORT_DECODE1D_NAME})
    set(DNN_BACKEND smmodel)

elseif(WITH_HAILO)
    set(DET_MODEL_URL "${DEEPLEARNING_PATH}/hailo/model/${HAILO_DET_MODEL_NAME}")
    set(DECODE_MODEL_URL "")
    set(DET_MODEL_NAME ${HAILO_DET_MODEL_NAME})
    # set(DECODE1D_MODEL_NAME ${HAILO_DECODE1D_NAME})
    set(DNN_BACKEND hailo_model)
elseif(WITH_NOVAIC)
    set(DET_MODEL_URL "${X_DEEPLEARNING_PATH}/novaic/${NOVAIC_DET_MODEL_NAME}")
    set(DECODE_MODEL_URL "")
    set(DET_MODEL_NAME ${NOVAIC_DET_MODEL_NAME})
    set(DNN_BACKEND novaic_model)
elseif(WITH_RKNN)
    set(DET_MODEL_URL "${X_DEEPLEARNING_PATH}/rknn/${RKNN_DET_MODEL_NAME}")
    set(DECODE_MODEL_URL "")
    set(DET_MODEL_NAME ${RKNN_DET_MODEL_NAME})
    set(DNN_BACKEND rknn_model)
else()
    message(FATAL_ERROR "Only support tengine,onnxruntime,tensorrt or hailo backend")

endif() ## if(${WITH_TENGINE})

set(MODELFILES_LOCAL_PATH ${MODELFILES_LOCAL_PATH}/${DNN_BACKEND})

############################################################################################
message(STATUS "MODELFILES_LOCAL_PATH ----> ${MODELFILES_LOCAL_PATH}")
message(STATUS "DET_MODEL_URL ----> ${DET_MODEL_URL}")
if(EXISTS ${MODELFILES_LOCAL_PATH})
    set(DET_LOCAL_DET_MODELS "${MODELFILES_LOCAL_PATH}/${DET_MODEL_NAME}")
    set(DECODE1D_LOCAL_MODELS "${MODELFILES_LOCAL_PATH}/${DECODE1D_MODEL_NAME}")

    message("DET_LOCAL_DET_MODELS ----> ${DET_LOCAL_DET_MODELS}")
    message("DECODE1D_LOCAL_MODELS ----> ${DECODE1D_LOCAL_MODELS}")
else()
    include(FetchContent)
    FetchContent_Declare(${DNN_BACKEND}
        URL ${DET_MODEL_URL}
        SOURCE_DIR  ${MODELFILES_LOCAL_PATH}
        DOWNLOAD_NO_EXTRACT ${NO_EXTEACT}   # If you don't want to  unpack, set NO_EXTEACT TRUE
    )
    FetchContent_MakeAvailable(${DNN_BACKEND} )
endif()

message("DNN_BACKEND ----> ${DNN_BACKEND}")
message("DNN_BACKEND_PATH ----> ${DNN_BACKEND_PATH}")
install(DIRECTORY ${MODELFILES_LOCAL_PATH} DESTINATION ./${TARGET_ARCHITECTURE}/3rdparty)

if(WITH_HAILO)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/3rdparty/tengine_model DESTINATION ./${TARGET_ARCHITECTURE}/3rdparty)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/3rdparty/tengine_fine_model DESTINATION ./${TARGET_ARCHITECTURE}/3rdparty)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/3rdparty/tengine_fpcls_model DESTINATION ./${TARGET_ARCHITECTURE}/3rdparty)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/3rdparty/tengine_binary_model DESTINATION ./${TARGET_ARCHITECTURE}/3rdparty)
endif()

if(WITH_TENGINE)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/3rdparty/tengine_fine_model DESTINATION ./${TARGET_ARCHITECTURE}/3rdparty)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/3rdparty/tengine_fpcls_model DESTINATION ./${TARGET_ARCHITECTURE}/3rdparty)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/3rdparty/tengine_versioncls_model DESTINATION ./${TARGET_ARCHITECTURE}/3rdparty)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/3rdparty/tengine_binary_model DESTINATION ./${TARGET_ARCHITECTURE}/3rdparty)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/3rdparty/tengine_1d_dl_decoder_model DESTINATION ./${TARGET_ARCHITECTURE}/3rdparty)
endif()


if(WITH_NOVAIC)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/3rdparty/novaic_model DESTINATION ./${TARGET_ARCHITECTURE}/3rdparty)
endif()

if(WITH_RKNN)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/3rdparty/rknn_model DESTINATION ./${TARGET_ARCHITECTURE}/3rdparty)
endif()

