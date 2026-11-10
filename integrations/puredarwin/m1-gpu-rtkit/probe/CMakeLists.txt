# SPDX-License-Identifier: GPL-2.0
# PDRtkitProbe: the Apple mailbox + RTKit port (src/, copied here as rtkit/ by
# ../build-probe-kext.nix) on the layer exactly as a LinuxKPI DRM kext links it, the way
# PDRustLKPI is built (../../m1-rust-kext/lkpi/pdrustlkpi/kext/CMakeLists.txt): Haiku's
# FreeBSD compatibility code, FreeBSD LinuxKPI with the DRM additions and PureDarwin glue,
# all hidden visibility, same defines as the Rust bindings (pd_drm_layout).
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/drm/drm.cmake")

pd_drm_layout("${PD_DRMKMOD_DIR}" drminc drmdefs drmopts)
pd_drm_core_includes("${PD_DRMKMOD_DIR}" coreinc)

set(RTKIT "${CMAKE_CURRENT_SOURCE_DIR}/rtkit")

add_fbsdkpi_driver(PDRtkitProbe
    CLASS PDRtkitProbe
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDRtkitProbe
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    LINUXKPI_DRM
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glue.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/probe.c"
        "${RTKIT}/mailbox.c"
        "${RTKIT}/rtkit.c"
        "${RTKIT}/rtkit-crashlog.c"
        "${RTKIT}/pd_rtkit_compat.c"
        # LinuxKPI built with PD_LKPI_DRM prints through this (linux %p extensions)
        "${PD_DRM_DIR}/src/pd_drm_printf.c"
    LKPI_INCLUDES ${drminc}
    LKPI_DEFINITIONS ${drmdefs} KBUILD_MODNAME=\"linuxkpi\"
    LKPI_OPTIONS ${drmopts}
    DEFINITIONS ${drmdefs} KBUILD_MODNAME=\"rtkitprobe\"
    # the port's own linux/soc/apple/*.h first
    INCLUDES "${RTKIT}/include" "${RTKIT}" "${CMAKE_CURRENT_SOURCE_DIR}" ${drminc} ${coreinc}
    XNU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/PDRtkitProbe.cpp"
        "${RTKIT}/PDRtkitPlatform.cpp"
)
target_include_directories(PDRtkitProbe.kext PRIVATE "${RTKIT}" "${CMAKE_CURRENT_SOURCE_DIR}")
