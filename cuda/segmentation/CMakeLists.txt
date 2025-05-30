set(SUBSYS_NAME cuda_segmentation)
set(SUBSYS_PATH cuda/segmentation)
set(SUBSYS_DESC "Point cloud CUDA Segmentation library")
set(SUBSYS_DEPS cuda_common io common)

# ---[ Point Cloud Library - pcl/cuda/io

PCL_SUBSYS_OPTION(build "${SUBSYS_NAME}" "${SUBSYS_DESC}" ON)
mark_as_advanced("BUILD_${SUBSYS_NAME}")
PCL_SUBSYS_DEPEND(build NAME ${SUBSYS_NAME} DEPS ${SUBSYS_DEPS})
PCL_SET_SUBSYS_INCLUDE_DIR("${SUBSYS_NAME}" "${SUBSYS_PATH}")


if(NOT build)
  return()
endif()

set(incs
  include/pcl/cuda/segmentation/connected_components.h
)

set(srcs
  src/connected_components.cu
)

set(LIB_NAME "pcl_${SUBSYS_NAME}")
PCL_CUDA_ADD_LIBRARY(${LIB_NAME} COMPONENT ${SUBSYS_NAME} SOURCES ${srcs} ${incs})
target_link_libraries(${LIB_NAME} pcl_common pcl_io pcl_cuda_common)

set(EXT_DEPS "")
#set(EXT_DEPS CUDA)
PCL_MAKE_PKGCONFIG(${LIB_NAME} COMPONENT ${SUBSYS_NAME} DESC ${SUBSYS_DESC} PCL_DEPS ${SUBSYS_DEPS} EXT_DEPS ${EXT_DEPS})

# Install include files
PCL_ADD_INCLUDES("${SUBSYS_NAME}" "${SUBSYS_PATH}" ${incs})
