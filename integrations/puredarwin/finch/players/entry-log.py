#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Opt-in process-local stderr capture in the original GPL VLC entry source."""
from pathlib import Path
import sys
p=Path(sys.argv[1]);s=p.read_text();old='int main(int i_argc, const char *ppsz_argv[])\n{'
new=old+'''
    /* SiliconForest: explicit own-stage log path, no global logger changes. */
    const char *sf_log = getenv("SF_VLC_LOG");
    if (sf_log && *sf_log) {
        if (!freopen(sf_log, "a", stderr)) return 1;
        setvbuf(stderr, NULL, _IONBF, 0);
        fprintf(stderr, "SF_VLC_ENTRY pid=%d argc=%d\\n", getpid(), i_argc);
    }
'''
assert s.count(old)==1;s=s.replace(old,new).replace('#include <stdlib.h>','#include <stdlib.h>\n#include <stdio.h>',1);anchors={
 '    /* Handle parameters */':'    if (sf_log) fprintf(stderr, "SF_VLC_STAGE signals-ready\\n");\n',
 '    /* Initialize libvlc */':'    if (sf_log) fprintf(stderr, "SF_VLC_STAGE before-libvlc-new argc=%d\\n", argc);\n',
 '    if (vlc == NULL)':'    if (sf_log) fprintf(stderr, "SF_VLC_STAGE libvlc-new result=%p\\n", (void *)vlc);\n    if (sf_log && !vlc) { const char *sf_error=libvlc_errmsg(); fprintf(stderr, "SF_VLC_ERROR %s\\n", sf_error ? sf_error : "no public error supplied"); }\n',
 '    libvlc_add_intf(vlc, "hotkeys,none");':'    if (sf_log) fprintf(stderr, "SF_VLC_STAGE before-interface\\n");\n',
 '    libvlc_playlist_play(vlc, -1, 0, NULL);':'    if (sf_log) fprintf(stderr, "SF_VLC_STAGE interface-ready\\n");\n',
}
for anchor,addition in anchors.items():
    assert s.count(anchor)==1
    s=s.replace(anchor,addition+anchor)
p.write_text(s)
