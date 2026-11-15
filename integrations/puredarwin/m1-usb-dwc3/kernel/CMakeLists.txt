# SPDX-License-Identifier: GPL-2.0
# Object library for the upcoming Apple DWC3/xHCI integration. It is not an
# attached kext or a success stub for the missing host-controller lifecycle.
add_library(PDUSBDma OBJECT pd_usb_dart.cpp pd_dart_table.c pd_soc.cpp)
target_include_directories(PDUSBDma PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PDUSBDma PRIVATE xnu_kernel_private_headers)
target_compile_options(PDUSBDma PRIVATE -Os -Wall -Wno-inconsistent-missing-override)
set_property(TARGET PDUSBDma PROPERTY CXX_STANDARD 14)
