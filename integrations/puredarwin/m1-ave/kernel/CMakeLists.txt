# SPDX-License-Identifier: GPL-2.0-only
add_kext_bundle(PDAppleAVE.kext INFO_PLIST Info.plist KERNEL_PRIVATE
 BUNDLE_IDENTIFIER org.puredarwin.driver.PDAppleAVE BUNDLE_VERSION 0.1.0)
set_property(TARGET PDAppleAVE.kext PROPERTY OUTPUT_NAME PDAppleAVE)
target_sources(PDAppleAVE.kext PRIVATE PDAppleAVE.cpp pdave_identity.c)
target_include_directories(PDAppleAVE.kext PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(PDAppleAVE.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDAppleAVE.kext PRIVATE -Wall -Os)
set_property(TARGET PDAppleAVE.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDAppleAVE.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
