# SPDX-License-Identifier: GPL-2.0
# PDAgx: the Asahi AGX driver (Linux 7.2.11 drivers/gpu/drm/asahi, common code + G13) as a
# LinuxKPI DRM kext. In the bundle:
#   - the layer as every LinuxKPI DRM kext links it (Haiku FreeBSD-compat, FreeBSD LinuxKPI,
#     PureDarwin glue), hidden visibility
#   - the DRM core at Linux 7.2 (m1-drm72) with the GPL-2.0 GEM shmem helper
#   - the Apple mailbox/RTKit port (m1-gpu-rtkit/src, copied here as rtkit/)
#   - rust/asahi.o: the Rust `kernel` crate, the driver and the C helpers they call, one
#     prelinked object (lkpi/default.nix `agxObject`)
#   - the tree's IOKit class for a device-tree platform device, unchanged, attaching only
#     with the boot-arg pdagx=1
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/drm/drm.cmake")

# Size: the collection has 16 MiB for all kexts. The tree compiles kext C without -O; the DRM
# core, the RTKit port and this kext's glue get -Os here (Linux builds them -O2), and the
# link drops local symbols from the symbol table (-x: everything in the layer is hidden
# visibility, so that is nearly all of __LINKEDIT). The layer itself is compiled as in
# every other kext.
set(PDAGX_C_OPT -Os)
set(drmkmod "${PD_DRMKMOD_DIR}")
pd_drm_layout("${drmkmod}" drminc drmdefs drmopts)
pd_drm_core_includes("${drmkmod}" coreinc)
set(RTKIT "${CMAKE_CURRENT_SOURCE_DIR}/rtkit")

# the same linuxkpi directories add_fbsdkpi_driver gives its own objects (as PDDRMmsm does)
set(lkpiinc ${drminc}
    "${FBSDKPI_DIR}/pd/lkpi-override"
    "${FBSDKPI_DIR}/freebsd/linuxkpi/common/include"
    "${FBSDKPI_DIR}/freebsd/linuxkpi/dummy/include"
    "${FBSDKPI_DIR}/pd/fbsd-extra"
    "${FBSDKPI_DIR}/freebsd"
    "${FBSDKPI_DIR}/haiku/freebsd_iflib/compat")
set(lkpiopt "SHELL:-include ${FBSDKPI_DIR}/pd/fbsd-extra/pd_lkpi_pre.h"
    ${drmopts} -Werror=implicit-function-declaration)

# the whole kext is GPL-2.0: the shmem helper the Rust gem::shmem wrapper needs is in
if(NOT "drm_gem_shmem_helper.c" IN_LIST PD_DRM_CORE_SOURCES)
    list(APPEND PD_DRM_CORE_SOURCES drm_gem_shmem_helper.c)
endif()
# Space (one collection has to hold PDAgx and PDDcp, each with its own DRM core): TTM and the
# DisplayPort/HDMI/DSC helper library are for drivers that use them; neither this driver nor
# the DRM core itself references them. PD_DRM_KEEP_ALL=ON builds the full list again.
option(PD_DRM_KEEP_ALL "Build TTM and the display helper library into the DRM core" OFF)
if(NOT PD_DRM_KEEP_ALL)
    list(FILTER PD_DRM_CORE_SOURCES EXCLUDE REGEX "^(ttm|display)/")
endif()
set(core)
foreach(src IN LISTS PD_DRM_CORE_SOURCES)
    list(APPEND core "${drmkmod}/drivers/gpu/drm/${src}")
endforeach()
foreach(src IN LISTS PD_DRM_DMABUF_SOURCES)
    list(APPEND core "${drmkmod}/drivers/dma-buf/${src}")
endforeach()
foreach(src IN LISTS PD_DRM_VIDEO_SOURCES)
    list(APPEND core "${drmkmod}/drivers/video/${src}")
endforeach()
foreach(src IN LISTS PD_DRM_PD_SOURCES)
    list(APPEND core "${PD_DRM_DIR}/src/${src}")
endforeach()
_fbsdkpi_objects(PDAgx_drm SOURCES ${core}
    INCLUDES ${lkpiinc} ${coreinc}
    DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${drmdefs}
        KBUILD_MODNAME=\"drm\" LINUXKPI_PARAM_PREFIX=drm_
    OPTIONS ${lkpiopt} ${PDAGX_C_OPT})

# mailbox + RTKit and this kext's LinuxKPI-side glue (pre-probe, the sysfs PD_STUB); none of
# the driver's own C (sysfs.c, g17/*.c) is built
# The RTKit receive queue is WQ_HIGHPRI on linux. m1-gpu-rtkit's compat header creates it
# with linux_create_workqueue_common(), dropping the flag; here that one call makes the
# layer's high-priority queue (layer patch 0025, boot-arg pdlkpi_wq_highpri=0 for the old way).
set_source_files_properties("${RTKIT}/rtkit.c" PROPERTIES
    COMPILE_DEFINITIONS "linux_create_workqueue_common=linux_create_workqueue_highpri")
_fbsdkpi_objects(PDAgx_agx SOURCES
        "${RTKIT}/mailbox.c" "${RTKIT}/rtkit.c" "${RTKIT}/rtkit-crashlog.c" "${RTKIT}/pd_rtkit_compat.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/pd_agx_linux.c"
    INCLUDES "${RTKIT}/include" "${RTKIT}" ${lkpiinc} ${coreinc}
    DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${drmdefs}
        KBUILD_MODNAME=\"asahi\" LINUXKPI_PARAM_PREFIX=asahi_
    OPTIONS ${lkpiopt} ${PDAGX_C_OPT})

add_fbsdkpi_driver(PDAgx
    CLASS PDAgx
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDAgx
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    LINUXKPI_DRM
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glue.c"
    LKPI_INCLUDES ${drminc}
    LKPI_DEFINITIONS ${drmdefs} KBUILD_MODNAME=\"linuxkpi\"
    LKPI_OPTIONS ${drmopts}
    OBJECTS PDAgx_drm PDAgx_agx
    XNU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDDRMPlatformDevice.cpp"
        "${RTKIT}/PDRtkitPlatform.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/PDAgxHost.cpp"
)
# Space: the layer's own objects, compiled without -O by the tree, get -Os in this kext as they
# do in PDDcp; with both, PDAgx and PDDcp fit one collection with room for its prelink info.
# PDAGX_LAYER_O0=ON builds them as the tree does.
option(PDAGX_LAYER_O0 "Compile the layer objects of PDAgx without optimisation" OFF)
if(NOT PDAGX_LAYER_O0)
    foreach(t PDAgx_core PDAgx_lkpi PDAgx_lkpiglue PDAgx_driver PDAgx_iflib)
        if(TARGET ${t})
            target_compile_options(${t} PRIVATE -Os)
        endif()
    endforeach()
endif()

# default off: the driver attaches only when asked to with the pdagx=1 boot-arg
target_compile_definitions(PDAgx.kext PRIVATE PDDRM_OPT_IN_BOOTARG="pdagx"
    PDDRMFramebuffer=PDDRMFramebufferAgx PDDRMPresenter=PDDRMPresenterAgx)
target_include_directories(PDAgx.kext PRIVATE "${RTKIT}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../IOGraphicsFamily"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/drm/include")

target_link_options(PDAgx.kext PRIVATE -Wl,-x -Wl,-map,${CMAKE_BINARY_DIR}/PDAgx.map)

set(PDAGX_RUST_OBJECT ${CMAKE_CURRENT_SOURCE_DIR}/rust/asahi.o)
if(NOT EXISTS ${PDAGX_RUST_OBJECT})
    message(FATAL_ERROR "PDAgx: ${PDAGX_RUST_OBJECT} is missing (lkpi/default.nix `agxObject`)")
endif()
set_source_files_properties(${PDAGX_RUST_OBJECT} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED FALSE)
target_sources(PDAgx.kext PRIVATE ${PDAGX_RUST_OBJECT})
