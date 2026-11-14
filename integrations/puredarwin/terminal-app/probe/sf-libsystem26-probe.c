// SPDX-License-Identifier: MIT
// Runs only with a libSystem that has the names programs built against the macOS 26 SDK
// import (../../libsystem-default/patches/macos26-sdk-imports.patch): the installer's proof
// that a process gets the new library (../tools/sf_terminal.sh). Since the feature-flag reader
// (../../libsystem-default/patches/feature-flags.patch) it also needs os_feature_enabled to read
// the flag the tree installs (/System/Library/FeatureFlags/Domain/Terminal.plist: DirectColor on).
// Since memorystatus_get_level (../../libsystem-default/patches/memorystatus-level.patch, Activity
// Monitor's memory pressure) it imports that too, so the libSystem of before does not load it (the
// answer is printed, not judged: a kernel without system call 453 answers an error).
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
extern void *malloc_type_malloc(size_t size, unsigned long long type_id);
extern int os_variant_is_basesystem(const char *subsystem);
extern bool _os_feature_enabled_impl(const char *domain, const char *feature);
extern int memorystatus_get_level(uint64_t level);
int main(void)
{
    char *p = malloc_type_malloc(32, 0x1234);
    if (p == NULL) return 1;
    strcpy(p, "malloc_type_malloc");
    bool flag = _os_feature_enabled_impl("Terminal", "DirectColor");
    unsigned int level = 1000;
    int rc = memorystatus_get_level((uint64_t)(uintptr_t)&level);
    printf("sf-libsystem26-probe: %s answers, os_variant_is_basesystem=%d, feature Terminal/DirectColor=%d, memorystatus_get_level %d level %u\n",
           p, os_variant_is_basesystem("probe"), flag, rc, level);
    free(p);
    if (!flag && access("/System/Library/FeatureFlags/Domain/Terminal.plist", R_OK) == 0) return 2;
    return 0;
}
