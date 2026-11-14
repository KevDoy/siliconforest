// SPDX-License-Identifier: MIT
// Runs only with a libSystem that has the names programs built against the macOS 26 SDK
// import (../../libsystem-default/patches/macos26-sdk-imports.patch): the installer's proof
// that a process gets the new library (../tools/sf_terminal.sh).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern void *malloc_type_malloc(size_t size, unsigned long long type_id);
extern int os_variant_is_basesystem(const char *subsystem);
int main(void)
{
    char *p = malloc_type_malloc(32, 0x1234);
    if (p == NULL) return 1;
    strcpy(p, "malloc_type_malloc");
    printf("sf-libsystem26-probe: %s answers, os_variant_is_basesystem=%d\n", p, os_variant_is_basesystem("probe"));
    free(p);
    return 0;
}
