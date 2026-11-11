# SPDX-License-Identifier: GPL-2.0-only
add_kext_bundle(PDAppleTouchBar.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDAppleTouchBar BUNDLE_VERSION 0.1.0)
set_property(TARGET PDAppleTouchBar.kext PROPERTY OUTPUT_NAME PDAppleTouchBar)
target_sources(PDAppleTouchBar.kext PRIVATE PDAppleTouchBar.cpp pd_soc.cpp pd_gpio.cpp pd_dart_fixed.cpp
    pd_adp.cpp pd_spi.cpp pd_z2.cpp pd_tb_keys.c)
target_include_directories(PDAppleTouchBar.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/IOHIDSystem
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/IOHIDFamily
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily)
target_link_libraries(PDAppleTouchBar.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDAppleTouchBar.kext PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/include/pd_hid_compat.h>"
    -Wall -Wno-inconsistent-missing-override)
set_property(TARGET PDAppleTouchBar.kext PROPERTY C_STANDARD 11)
set_property(TARGET PDAppleTouchBar.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDAppleTouchBar.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
