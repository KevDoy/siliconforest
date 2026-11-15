# SPDX-License-Identifier: GPL-2.0
add_kext_bundle(PDAppleTypeC.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDAppleTypeC BUNDLE_VERSION 0.1.0)
set_property(TARGET PDAppleTypeC.kext PROPERTY OUTPUT_NAME PDAppleTypeC)
target_sources(PDAppleTypeC.kext PRIVATE PDAppleTypeC.cpp pd_i2c.cpp pd_soc.cpp)
target_include_directories(PDAppleTypeC.kext PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PDAppleTypeC.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDAppleTypeC.kext PRIVATE -Os -Wall -Wextra -Wno-inconsistent-missing-override)
set_property(TARGET PDAppleTypeC.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDAppleTypeC.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
