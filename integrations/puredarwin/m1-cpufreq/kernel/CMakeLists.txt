# SPDX-License-Identifier: GPL-2.0-only
add_kext_bundle(PDCpuPower.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDCpuPower BUNDLE_VERSION 0.1.0)
set_property(TARGET PDCpuPower.kext PROPERTY OUTPUT_NAME PDCpuPower)
target_sources(PDCpuPower.kext PRIVATE PDCpuPower.cpp PDSmcSession.cpp pd_cpufreq_policy.c pd_thermal_policy.c)
target_include_directories(PDCpuPower.kext PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PDCpuPower.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDCpuPower.kext PRIVATE -Wall -Wno-inconsistent-missing-override)
set_property(TARGET PDCpuPower.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDCpuPower.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
