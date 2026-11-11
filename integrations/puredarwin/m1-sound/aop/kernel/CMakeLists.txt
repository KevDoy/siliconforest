# SPDX-License-Identifier: GPL-2.0-only
add_kext_bundle(PDAopAudio.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDAopAudio BUNDLE_VERSION 0.1.0)
set_property(TARGET PDAopAudio.kext PROPERTY OUTPUT_NAME PDAopAudio)
target_sources(PDAopAudio.kext PRIVATE PDAopAudio.cpp pd_aop.cpp pd_admac.cpp pd_soc.cpp pd_dart_fixed.cpp)
target_include_directories(PDAopAudio.kext PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PDAopAudio.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDAopAudio.kext PRIVATE -Wall -Wno-inconsistent-missing-override)
set_property(TARGET PDAopAudio.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDAopAudio.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
