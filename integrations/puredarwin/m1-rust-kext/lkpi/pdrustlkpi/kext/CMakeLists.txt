# SPDX-License-Identifier: GPL-2.0
# PDRustLKPI: the layer exactly as a LinuxKPI DRM kext links it (Haiku's FreeBSD
# compatibility code, FreeBSD LinuxKPI with the DRM additions, PureDarwin glue, all compiled
# into this kext with hidden visibility), without a drm core or a driver, plus one
# prelinked object: the Rust `kernel` crate, the test crate and the C helpers they call
# (rust/pdrustlkpi.o, from ../../default.nix through ../../../build-lkpi-kext.nix).
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/drm/drm.cmake")

# the struct layouts of a drm kext: the Rust bindings were generated with these defines
pd_drm_layout("${PD_DRMKMOD_DIR}" drminc drmdefs drmopts)
pd_drm_core_includes("${PD_DRMKMOD_DIR}" coreinc)

add_fbsdkpi_driver(PDRustLKPI
    CLASS PDRustLKPI
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDRustLKPI
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    LINUXKPI_DRM
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glue.c"
        # LinuxKPI built with PD_LKPI_DRM prints through this (linux %p extensions)
        "${PD_DRM_DIR}/src/pd_drm_printf.c"
    LKPI_INCLUDES ${drminc}
    LKPI_DEFINITIONS ${drmdefs} KBUILD_MODNAME=\"linuxkpi\"
    LKPI_OPTIONS ${drmopts}
    DEFINITIONS ${drmdefs} KBUILD_MODNAME=\"rustlkpi\"
    INCLUDES ${drminc} ${coreinc}
    XNU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/PDRustLKPI.cpp"
)

set(PDRUSTLKPI_RUST_OBJECT ${CMAKE_CURRENT_SOURCE_DIR}/rust/pdrustlkpi.o)
if(NOT EXISTS ${PDRUSTLKPI_RUST_OBJECT})
    message(FATAL_ERROR "PDRustLKPI: ${PDRUSTLKPI_RUST_OBJECT} is missing (lkpi/default.nix `object`)")
endif()
set_source_files_properties(${PDRUSTLKPI_RUST_OBJECT} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED FALSE)
target_sources(PDRustLKPI.kext PRIVATE ${PDRUSTLKPI_RUST_OBJECT})
# No -dead_strip here (unlike PDRustHello): the layer's code is hidden-visibility and its
# SYSINITs are reached only through their section, so the linker would see them as dead.
