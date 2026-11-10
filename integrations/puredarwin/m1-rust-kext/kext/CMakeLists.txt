# SPDX-License-Identifier: GPL-2.0
# PDRustHello: a C++ IOKit shell plus one prelinked Rust object (rust/pdrusthello.o, put
# here by ../build-kext.nix from ../build-rust.nix). The Rust side is not compiled by
# CMake: its target and flags live in ../rust/toolchain.nix.
add_kext_bundle(PDRustHello.kext INFO_PLIST Info.plist
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDRustHello BUNDLE_VERSION 0.1.0)
set_property(TARGET PDRustHello.kext PROPERTY OUTPUT_NAME PDRustHello)
target_sources(PDRustHello.kext PRIVATE PDRustHello.cpp)
target_include_directories(PDRustHello.kext PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(PDRustHello.kext PRIVATE -Wall -Wno-inconsistent-missing-override)
set_property(TARGET PDRustHello.kext PROPERTY CXX_STANDARD 14)

set(PDRUSTHELLO_RUST_OBJECT ${CMAKE_CURRENT_SOURCE_DIR}/rust/pdrusthello.o)
if(NOT EXISTS ${PDRUSTHELLO_RUST_OBJECT})
    message(FATAL_ERROR "PDRustHello: ${PDRUSTHELLO_RUST_OBJECT} is missing (build-rust.nix output)")
endif()
set_source_files_properties(${PDRUSTHELLO_RUST_OBJECT} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED FALSE)
target_sources(PDRustHello.kext PRIVATE ${PDRUSTHELLO_RUST_OBJECT})
# core and alloc are linked whole into that object; drop what nothing reaches. Roots are the
# kext's global symbols (the C++ side, kmod_info), so only unreferenced Rust code goes.
target_link_options(PDRustHello.kext PRIVATE "LINKER:-dead_strip")

install(TARGETS PDRustHello.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
