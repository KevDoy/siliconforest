# SPDX-License-Identifier: GPL-2.0
# PDRustPlat: the layer as a LinuxKPI DRM kext links it, the tree's own IOKit class for a
# device-tree platform device (fbsdkpi/xnu/PDDRMPlatformDevice.cpp, unchanged), and one
# prelinked object: the Rust `kernel` crate with its driver-core modules, the test driver and
# the C helpers they call (rust/pdrustplat.o, from ../../default.nix).
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/drm/drm.cmake")

pd_drm_layout("${PD_DRMKMOD_DIR}" drminc drmdefs drmopts)
pd_drm_core_includes("${PD_DRMKMOD_DIR}" coreinc)

add_fbsdkpi_driver(PDRustPlat
    CLASS PDRustPlat
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDRustPlat
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    LINUXKPI_DRM
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glue.c"
        "${PD_DRM_DIR}/src/pd_drm_printf.c"
    LKPI_INCLUDES ${drminc}
    LKPI_DEFINITIONS ${drmdefs} KBUILD_MODNAME=\"linuxkpi\"
    LKPI_OPTIONS ${drmopts}
    DEFINITIONS ${drmdefs} KBUILD_MODNAME=\"rustplat\"
    INCLUDES ${drminc} ${coreinc}
    XNU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDDRMPlatformDevice.cpp"
)

set(PDRUSTPLAT_RUST_OBJECT ${CMAKE_CURRENT_SOURCE_DIR}/rust/pdrustplat.o)
if(NOT EXISTS ${PDRUSTPLAT_RUST_OBJECT})
    message(FATAL_ERROR "PDRustPlat: ${PDRUSTPLAT_RUST_OBJECT} is missing (lkpi/default.nix `platObject`)")
endif()
set_source_files_properties(${PDRUSTPLAT_RUST_OBJECT} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED FALSE)
target_sources(PDRustPlat.kext PRIVATE ${PDRUSTPLAT_RUST_OBJECT})
