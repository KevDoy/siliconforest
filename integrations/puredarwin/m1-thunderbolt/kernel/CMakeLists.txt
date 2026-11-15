# SPDX-License-Identifier: GPL-2.0
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/drm/drm.cmake")
pd_drm_layout("${PD_DRMKMOD_DIR}" drminc drmdefs drmopts)
set(RTKIT "${CMAKE_CURRENT_SOURCE_DIR}/rtkit")
add_fbsdkpi_driver(PDAppleCIO CLASS PDAppleCIO
 BUNDLE_IDENTIFIER org.puredarwin.driver.PDAppleCIO
 INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist" LINUXKPI_DRM
 SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glue.c" "${CMAKE_CURRENT_SOURCE_DIR}/pd_cio_rtkit.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/pd_dart_table.c" "${CMAKE_CURRENT_SOURCE_DIR}/pd_apple_nhi.c" "${CMAKE_CURRENT_SOURCE_DIR}/pd_cio_tunables.c"
  "${RTKIT}/mailbox.c" "${RTKIT}/rtkit.c" "${RTKIT}/rtkit-crashlog.c" "${RTKIT}/pd_rtkit_compat.c"
  "${PD_DRM_DIR}/src/pd_drm_printf.c"
 LKPI_INCLUDES ${drminc} LKPI_DEFINITIONS ${drmdefs} KBUILD_MODNAME=\"linuxkpi\"
 LKPI_OPTIONS ${drmopts}
 DEFINITIONS ${drmdefs} KBUILD_MODNAME=\"applecio\"
 INCLUDES "${RTKIT}/include" "${RTKIT}" "${CMAKE_CURRENT_SOURCE_DIR}" ${drminc}
 XNU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/PDAppleCIO.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/pd_cio_dart.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/pd_soc.cpp" "${RTKIT}/PDRtkitPlatform.cpp")
target_include_directories(PDAppleCIO.kext PRIVATE "${RTKIT}" "${CMAKE_CURRENT_SOURCE_DIR}")
