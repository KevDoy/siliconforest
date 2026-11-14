#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Derive method declarations from pinned Finch implementations, never an SDK.

Used to author the small framework headers. Types, numeric enums, inheritance,
protocols and external dependencies are supplied separately and reviewed.
"""
import argparse
from pathlib import Path
import re


def declarations(source, classes):
    text = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    for impl in re.finditer(r'@implementation\s+(\w+)([^\n{]*)', text):
        name, suffix = impl.groups()
        if '(' in suffix or name not in classes:
            continue
        end = text.index('@end', impl.end())
        body = text[impl.end():end]
        methods = re.findall(r'^[ \t]*([+-]\s*\([^\n]+?\)[^{;]*?)\s*\{', body, re.M)
        for method in methods:
            first = re.match(r'[+-]\s*\([^)]*\)\s*(\w+)', method).group(1)
            if re.search(r'^[ \t]*[+-]\s*\([^)]*\)\s*'+first+r'[^;{]*;', text[:impl.start()], re.M):
                continue
            classes[name].append(re.sub(r'\s+', ' ', method.strip())+';')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('source', type=Path)
    ap.add_argument('preamble', type=Path)
    ap.add_argument('output', type=Path)
    ap.add_argument('--class-map', required=True, help='Class:Superclass,Class:Superclass')
    args = ap.parse_args()
    bases = dict(pair.split(':') for pair in args.class_map.split(','))
    classes = {name: [] for name in bases}
    texts = [p.read_text() for p in sorted(args.source.glob('*.m'))]
    for text in texts:
        declarations(text, classes)
    constants = []
    for text in texts:
        for typ, name in re.findall(r'^(NSString\s*\*\s*const|const\s+double)\s+(\w+)\s*=', text, re.M):
            constants.append('extern '+typ+' '+name+';')
    result = ['/* SPDX-License-Identifier: MIT OR Apache-2.0 */\n',
              '/* Method signatures derived from Finch 2d9b1bb implementations.\n',
              ' * Type definitions and inheritance supplied by SiliconForest; no SDK headers. */\n',
              '#pragma once\n#import <Foundation/Foundation.h>\n#import <dispatch/dispatch.h>\n',
              '@class '+', '.join(classes)+';\n', args.preamble.read_text(), '\n',
              '\n'.join(constants)+'\n']
    for name, superclass in bases.items():
        result += ['\n@interface '+name+' : '+superclass+'\n', '\n'.join(classes[name]), '\n@end\n']
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(''.join(result))


if __name__ == '__main__':
    main()
