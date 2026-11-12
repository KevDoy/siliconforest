# SPDX-License-Identifier: GPL-2.0-only
add_kext_bundle(PDAppleNVMe.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDAppleNVMe BUNDLE_VERSION 0.1.0)
set_property(TARGET PDAppleNVMe.kext PROPERTY OUTPUT_NAME PDAppleNVMe)
target_sources(PDAppleNVMe.kext PRIVATE PDAppleNVMe.cpp pd_ans.cpp pd_nvme_gate.c pd_nvme_part.c pd_soc.cpp)
target_include_directories(PDAppleNVMe.kext PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOStorageFamily/include)
target_link_libraries(PDAppleNVMe.kext PRIVATE xnu_kernel_private_headers)
# -Os: the collection has 0x2000 bytes to spare before the kernel image, and members are padded to 16 KiB per segment
target_compile_options(PDAppleNVMe.kext PRIVATE -Wall -Wno-inconsistent-missing-override -Os)
set_property(TARGET PDAppleNVMe.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDAppleNVMe.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
