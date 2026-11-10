# SPDX-License-Identifier: MIT
add_kext_bundle(PDGpuDT.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDGpuDT BUNDLE_VERSION 0.1.0)
set_property(TARGET PDGpuDT.kext PROPERTY OUTPUT_NAME PDGpuDT)
target_sources(PDGpuDT.kext PRIVATE PDGpuDT.cpp gpudt.c gpudt_summary.c PDDispDT.cpp dispdt.c)
target_include_directories(PDGpuDT.kext PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PDGpuDT.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDGpuDT.kext PRIVATE -Wall -Wno-inconsistent-missing-override)
set_property(TARGET PDGpuDT.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDGpuDT.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
