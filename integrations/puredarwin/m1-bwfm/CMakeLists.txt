# PDBwfm: Broadcom BCM4378 (14e4:4425) PCIe FullMAC 802.11ax of the Apple M1 on IO80211Family.
# The radio is OpenBSD's bwfm(4), ISC licensed (see the file headers and OPENBSD_REVISION).
# Firmware is not built in, it loads from /Library/Firmware/brcm under the linux names.
add_kext_bundle(PDBwfm.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDBwfm BUNDLE_VERSION 1.0.0)
set_property(TARGET PDBwfm.kext PROPERTY OUTPUT_NAME PDBwfm)

set(PDBWFM_C_SOURCES bwfm.c if_bwfm_pci.c pd_bwfm_glue.c)
# pd_dart_map.cpp (per-buffer DART mapping, the default) and pd_dart_pool.cpp (the fixed bounce
# pool) are GPL-2.0, from ../m1-apcie-host/dart; they make this kext GPL-2.0 as a whole
target_sources(PDBwfm.kext PRIVATE ${PDBWFM_C_SOURCES} pd_dart_map.cpp pd_dart_pool.cpp PDBwfm.cpp)
# DMA switches, all off by default:
#   PD_BWFM_DMA_BOUNCE    the fixed bounce pool instead of per-buffer mapping
#   PD_BWFM_DMA_RX_SLOTS  per-buffer mapping, but receive goes through permanently mapped slots
#   PD_DART_STRICT        invalidate the DART TLB in every map and unmap, not once per ring commit
foreach(pd_bwfm_switch PD_BWFM_DMA_BOUNCE PD_BWFM_DMA_RX_SLOTS PD_DART_STRICT)
    option(${pd_bwfm_switch} "PDBwfm: see CMakeLists.txt" OFF)
    if(${pd_bwfm_switch})
        target_compile_definitions(PDBwfm.kext PRIVATE ${pd_bwfm_switch})
    endif()
endforeach()
set_source_files_properties(${PDBWFM_C_SOURCES} PROPERTIES COMPILE_OPTIONS
    "-include;${CMAKE_CURRENT_SOURCE_DIR}/../IO80211Family/compat/pd80211_compat.h;-Wno-sign-compare;-Wno-unused-parameter;-Wno-unused-function;-Wno-unused-variable;-Wno-unused-but-set-variable;-Wno-strict-prototypes;-Wno-missing-field-initializers;-Wno-shadow;-Wno-cast-qual;-Wno-pointer-sign;-Wno-visibility;-ferror-limit=0")

target_include_directories(PDBwfm.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IO80211Family
    ${CMAKE_CURRENT_SOURCE_DIR}/../IO80211Family/compat
    ${CMAKE_CURRENT_BINARY_DIR}/../IO80211Family/shim-include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOPCIFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/EXTERNAL_HEADERS)
target_link_libraries(PDBwfm.kext PRIVATE xnu_kernel_private_headers)
target_compile_definitions(PDBwfm.kext PRIVATE __PRIVATE_SPI__)
target_compile_options(PDBwfm.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDBwfm.kext PROPERTY CXX_STANDARD 14)
add_dependencies(PDBwfm.kext IO80211Family.kext)
install(TARGETS PDBwfm.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
