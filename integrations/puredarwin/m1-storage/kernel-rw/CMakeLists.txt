# SPDX-License-Identifier: GPL-2.0-only
# PDAppleNVMeRW: the WRITE-FENCED build of the internal NVMe driver. Its sources are ../kernel
# with rw.patch applied (see ../tools/rw_src.sh) plus pd_nvme_wgate.c and pd_nvme_wring.c.
# Never in a default collection.
add_kext_bundle(PDAppleNVMeRW.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDAppleNVMeRW BUNDLE_VERSION 0.1.0)
set_property(TARGET PDAppleNVMeRW.kext PROPERTY OUTPUT_NAME PDAppleNVMeRW)
target_sources(PDAppleNVMeRW.kext PRIVATE PDAppleNVMe.cpp pd_ans.cpp pd_nvme_gate.c pd_nvme_wgate.c pd_nvme_wring.c pd_nvme_part.c pd_soc.cpp)
target_include_directories(PDAppleNVMeRW.kext PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOStorageFamily/include)
target_link_libraries(PDAppleNVMeRW.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDAppleNVMeRW.kext PRIVATE -Wall -Wno-inconsistent-missing-override -Os)
set_property(TARGET PDAppleNVMeRW.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDAppleNVMeRW.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
