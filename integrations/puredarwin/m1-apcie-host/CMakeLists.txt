# SPDX-License-Identifier: GPL-2.0
add_kext_bundle(PDApcieT8103Bridge.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDApcieT8103Bridge BUNDLE_VERSION 0.2.0)
set_property(TARGET PDApcieT8103Bridge.kext PROPERTY OUTPUT_NAME PDApcieT8103Bridge)
target_sources(PDApcieT8103Bridge.kext PRIVATE PDApcieT8103.cpp PDApcieT8103Link.cpp PDAppleSMC.cpp)
target_include_directories(PDApcieT8103Bridge.kext PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PDApcieT8103Bridge.kext PRIVATE xnu_kernel_private_headers IOPCIFamilyHeaders)
target_compile_options(PDApcieT8103Bridge.kext PRIVATE -Wall -Wno-inconsistent-missing-override)
set_property(TARGET PDApcieT8103Bridge.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDApcieT8103Bridge.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
