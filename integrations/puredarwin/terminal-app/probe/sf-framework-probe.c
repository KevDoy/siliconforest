// SPDX-License-Identifier: MIT
// Linked against a framework wrapper of ../compat at its install path, importing a name only
// the wrapper has: it loads only where that path is the wrapper (a link to /usr/lib's library
// lacks the name). The installer's proof for CoreFoundation.framework and Foundation.framework.
#include <stdio.h>
#include <stdint.h>
#ifdef PROBE_COREFOUNDATION
extern void *CFStringTokenizerCreate(void *alloc, void *string, long location, long length, unsigned long options, void *locale);
extern unsigned long CFArrayGetTypeID(void);                 // from the re-exported library
int main(void) { printf("sf-corefoundation-framework-probe: the tokenizer is at %p, array type %lu\n", (void *)&CFStringTokenizerCreate, CFArrayGetTypeID()); return CFArrayGetTypeID() != 0 ? 0 : 1; }
#endif
#ifdef PROBE_FOUNDATION
extern void *NSFileTypeForHFSTypeCode(uint32_t code);
extern void *NSStringFromClass(void *cls);                   // from the re-exported library
int main(void) { void *s = NSFileTypeForHFSTypeCode(0x54455854); printf("sf-foundation-framework-probe: NSFileTypeForHFSTypeCode %s, NSStringFromClass %p\n", s ? "answers" : "nil", (void *)&NSStringFromClass); return s != 0 ? 0 : 1; }
#endif
