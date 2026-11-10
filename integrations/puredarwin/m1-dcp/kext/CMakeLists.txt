# SPDX-License-Identifier: GPL-2.0-only OR MIT
# PDDcp: Omacom's display-coprocessor driver (drivers/gpu/drm/apple at linux-aurora 6a08a08e)
# as a LinuxKPI DRM kext. In the bundle:
#   - the layer as every LinuxKPI DRM kext links it, hidden visibility
#   - the DRM core at Linux 7.2 (m1-drm72)
#   - driver/: the driver's sources with ../patches applied (build-kext.nix stages them)
#   - pin/: DRM helpers and the component framework the core lacks, from the pin as they are
#   - pininc/: headers from the pin (tools/pin-headers.txt)
#   - rtkit/: the Apple mailbox/RTKit port (m1-gpu-rtkit/src); dart/: m1-dcp-dart
#   - the tree's IOKit class for a device-tree platform device, attaching with pddcp=1|2
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/drm/drm.cmake")

set(PDDCP_C_OPT -Os)
set(drmkmod "${PD_DRMKMOD_DIR}")
pd_drm_layout("${drmkmod}" drminc drmdefs drmopts)
pd_drm_core_includes("${drmkmod}" coreinc)
set(HERE "${CMAKE_CURRENT_SOURCE_DIR}")
set(RTKIT "${HERE}/rtkit")
set(DART "${HERE}/dart")

set(lkpiinc ${drminc}
    "${FBSDKPI_DIR}/pd/lkpi-override"
    "${FBSDKPI_DIR}/freebsd/linuxkpi/common/include"
    "${FBSDKPI_DIR}/freebsd/linuxkpi/dummy/include"
    "${FBSDKPI_DIR}/pd/fbsd-extra"
    "${FBSDKPI_DIR}/freebsd"
    "${FBSDKPI_DIR}/haiku/freebsd_iflib/compat")
set(lkpiopt "SHELL:-include ${FBSDKPI_DIR}/pd/fbsd-extra/pd_lkpi_pre.h"
    ${drmopts} -Werror=implicit-function-declaration)

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
_fbsdkpi_objects(PDDcp_drm SOURCES ${core}
    INCLUDES ${lkpiinc} ${coreinc}
    DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${drmdefs}
        KBUILD_MODNAME=\"drm\" LINUXKPI_PARAM_PREFIX=drm_
    OPTIONS ${lkpiopt} ${PDDCP_C_OPT})

# the driver: the 27 files J293's internal panel can reach or that only cost space to keep.
# Not built: dcpext_scanout.c, dcpext_drm.c, j613_dcp_activate.c (pd_dcp_linux.c, part 3),
# audio.c, av.c (CONFIG_DRM_APPLE_AUDIO off) and the tests.
set(drv)
foreach(f apple_drv apple_resume afk dcp dcp-fabric dcp-fabric-core dcp_backlight dptxep iomfb parser systemep
        dcp-hdmi connector dcp-display-metadata dcp-180hz dcp-panel-edid ibootep
        iomfb_v12_3 iomfb_v13_3 iomfb_v14_7_tpl iomfb_v14_7 iomfb_v14_7_link iomfb_h17p
        iomfb_v26_6 iomfb_v26_6_link epic/dpavservep plane)
    list(APPEND drv "${HERE}/driver/${f}.c")
endforeach()
set(pin "${HERE}/pin/drm_gem_dma_helper.c" "${HERE}/pin/drm_fb_dma_helper.c"
    "${HERE}/pin/drm_gem_framebuffer_helper.c" "${HERE}/pin/drm_simple_kms_helper.c"
    "${HERE}/pin/component.c")
set(dcpopt ${lkpiopt}
    "SHELL:-include ${HERE}/include/pd_dcp_compat.h"
    "SHELL:-include ${DART}/include/pd_dcp_iommu.h" ${PDDCP_C_OPT} -Wno-everything
    -Werror=implicit-function-declaration)
# the layer's pd_drm_gem_helpers.c already has three functions of this file; the pin's copies
# keep to the file under other names
set_source_files_properties("${HERE}/pin/drm_gem_framebuffer_helper.c" PROPERTIES COMPILE_DEFINITIONS
    "drm_gem_fb_get_obj=pd_pin_drm_gem_fb_get_obj;drm_gem_fb_destroy=pd_pin_drm_gem_fb_destroy;drm_gem_fb_create_handle=pd_pin_drm_gem_fb_create_handle")
_fbsdkpi_objects(PDDcp_drv SOURCES ${drv} ${pin}
        "${HERE}/pd_dcp_linux.c"
        "${DART}/src/pd_dcp_iommu.c" "${DART}/src/pd_dart_table.c"
        "${RTKIT}/mailbox.c" "${RTKIT}/rtkit.c" "${RTKIT}/rtkit-crashlog.c" "${RTKIT}/pd_rtkit_compat.c"
    INCLUDES "${HERE}/pininc" "${HERE}/driver" "${HERE}/include" "${DART}/include"
        "${RTKIT}/include" "${RTKIT}" ${lkpiinc} ${coreinc}
    DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${drmdefs} CONFIG_MMU
        KBUILD_MODNAME=\"appledrm\" LINUXKPI_PARAM_PREFIX=appledrm_
    OPTIONS ${dcpopt})
# the 7.2 atomic names: compiled without the compat header's rename (see the file)
_fbsdkpi_objects(PDDcp_drm72 SOURCES "${HERE}/pd_dcp_drm72.c"
    INCLUDES "${HERE}/pininc" ${lkpiinc} ${coreinc}
    DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${drmdefs}
        KBUILD_MODNAME=\"appledrm\" LINUXKPI_PARAM_PREFIX=appledrm72_
    OPTIONS ${lkpiopt} ${PDDCP_C_OPT})

add_fbsdkpi_driver(PDDcp
    CLASS PDDcp
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDDcp
    INFO_PLIST "${HERE}/Info.plist"
    LINUXKPI_DRM
    SOURCES "${HERE}/glue.c"
    LKPI_INCLUDES ${drminc}
    LKPI_DEFINITIONS ${drmdefs} KBUILD_MODNAME=\"linuxkpi\"
    LKPI_OPTIONS ${drmopts}
    OBJECTS PDDcp_drm PDDcp_drv PDDcp_drm72
    XNU_SOURCES "${HERE}/../../fbsdkpi/xnu/PDDRMPlatformDevice.cpp"
        "${RTKIT}/PDRtkitPlatform.cpp"
        "${DART}/src/pd_dart.cpp"
        "${HERE}/PDDcpHost.cpp"
)
target_compile_definitions(PDDcp.kext PRIVATE PDDRM_OPT_IN_BOOTARG="pddcp"
    PDDRMFramebuffer=PDDRMFramebufferDcp PDDRMPresenter=PDDRMPresenterDcp)
target_include_directories(PDDcp.kext PRIVATE "${RTKIT}" "${DART}/include"
    "${HERE}/../IOGraphicsFamily" "${HERE}/../../fbsdkpi/drm/include")
target_link_options(PDDcp.kext PRIVATE -Wl,-x -Wl,-map,${CMAKE_BINARY_DIR}/PDDcp.map)
