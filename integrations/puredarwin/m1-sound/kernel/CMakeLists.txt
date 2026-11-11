# SPDX-License-Identifier: GPL-2.0-only
add_kext_bundle(PDAppleSound.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDAppleSound BUNDLE_VERSION 0.1.0)
set_property(TARGET PDAppleSound.kext PROPERTY OUTPUT_NAME PDAppleSound)
target_sources(PDAppleSound.kext PRIVATE PDAppleSound.cpp pd_soc.cpp pd_gpio.cpp pd_i2c.cpp pd_dart_fixed.cpp
    pd_mca.cpp pd_cs42l83.cpp pd_tas2770.cpp pd_speakers.cpp pd_speaker_model.c)
target_include_directories(PDAppleSound.kext PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PDAppleSound.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDAppleSound.kext PRIVATE -Wall -Wno-inconsistent-missing-override)
set_property(TARGET PDAppleSound.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDAppleSound.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
