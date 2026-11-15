#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Apply explicit native-only changes to a private copy, never the pin."""
import pathlib,re,sys
p=pathlib.Path(sys.argv[1]);names=set()
q=p/'domain.c';s=q.read_text();needle='int tb_domain_init(void)\n{';assert s.count(needle)==1
q.write_text(s.replace(needle,needle+'\n\tif (!pd_tb_native_ready()) return -EPROBE_DEFER;'))
# Intel x86 Apple NHI resume device-links do not apply to the ARM platform.
q=p/'tb.c';s=q.read_text();a=s.index('static bool tb_apple_add_links(struct tb_nhi *nhi)\n{');b=s.index('\nstruct tb *tb_probe',a)
s=s[:a]+'#ifdef CONFIG_X86\n'+s[a:b]+'\n#else\nstatic bool tb_apple_add_links(struct tb_nhi *nhi) { return false; }\n#endif\n'+s[b:];q.write_text(s)
q=p/'usb4_port.c';s=q.read_text();needle='device_unregister(&usb4->dev);\n\t}';assert s.count(needle)==1
q.write_text(s.replace(needle,'device_unregister(&usb4->dev);\n\t\treturn ERR_PTR(ret);\n\t}'))
for source in p.glob('*.c'):
 names.update(re.findall(r'\b(trace_tb_\w+)\s*\(',source.read_text()))
with (p/'trace.h').open('w') as f:
 f.write('/* SPDX-License-Identifier: GPL-2.0 */\n/* Linux tracepoint instrumentation disabled in the native port. */\n')
 for name in sorted(names):f.write('#define '+name+'(...) '+('false' if name.endswith('_enabled') else '((void)0)')+'\n')
