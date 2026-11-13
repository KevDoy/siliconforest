#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# sf_keys.py <items...> -> the request lines of the compositor's inject module (sf_inject.sh), separated by ";".
#   text:<string>   typed (US layout); \n is Return, \t Tab
#   key:<name>      return, backspace, tab, esc, up, down, left, right, space, pageup, pagedown, home, end, delete, f1..f12
#   chord:<mods>+<key>   mods of ctrl, shift, alt, cmd (e.g. chord:ctrl+c, chord:cmd+n)
#   wait:<ms>  capture:<name>  click:<x>,<y> (panel pixels of 2560x1600)  move:<x>,<y>  rclick:<x>,<y>  down:<x>,<y>  up:
import sys
L = {c: 0x04 + i for i, c in enumerate('abcdefghijklmnopqrstuvwxyz')}
D = {c: 0x1e + i for i, c in enumerate('1234567890')}
P = {'\n': 0x28, '\t': 0x2b, ' ': 0x2c, '-': 0x2d, '=': 0x2e, '[': 0x2f, ']': 0x30, '\\': 0x31, ';': 0x33, "'": 0x34, '`': 0x35, ',': 0x36, '.': 0x37, '/': 0x38}
S = dict(zip('!@#$%^&*()', '1234567890')); S.update({'_': '-', '+': '=', '{': '[', '}': ']', '|': '\\', ':': ';', '"': "'", '~': '`', '<': ',', '>': '.', '?': '/'})
N = dict(**{'return': 0x28, 'esc': 0x29, 'backspace': 0x2a, 'tab': 0x2b, 'space': 0x2c, 'right': 0x4f, 'left': 0x50, 'down': 0x51, 'up': 0x52,
            'pageup': 0x4b, 'pagedown': 0x4e, 'home': 0x4a, 'end': 0x4d, 'delete': 0x4c}, **{'f%d' % i: 0x39 + i for i in range(1, 13)})
M = {'ctrl': 0xe0, 'shift': 0xe1, 'alt': 0xe2, 'cmd': 0xe3}
out = []
def tap(u, mods=()):
    for m in mods: out.append('key %02x down' % M[m])
    out.append('key %02x down' % u); out.append('key %02x up' % u)
    for m in reversed(mods): out.append('key %02x up' % M[m])
    out.append('wait 12')
def usage(c):
    if c in L: return L[c], ()
    if c.lower() in L and c.isupper(): return L[c.lower()], ('shift',)
    if c in D: return D[c], ()
    if c in P: return P[c], ()
    if c in S: return usage(S[c])[0], ('shift',)
    raise SystemExit('no key for %r' % c)
def at(x, y): return 'abs %d %d' % (round(x * 65535 / 2559), round(y * 65535 / 1599))
for item in sys.argv[1:]:
    kind, _, arg = item.partition(':')
    if kind == 'text':
        for c in arg.replace('\\n', '\n').replace('\\t', '\t'): tap(*usage(c))
    elif kind == 'key': tap(N[arg])
    elif kind == 'chord':
        *mods, k = arg.split('+'); tap(N[k] if k in N else usage(k)[0], tuple(mods))
    elif kind == 'wait': out.append('wait ' + arg)
    elif kind == 'capture': out.append('capture ' + arg)
    elif kind in ('click', 'rclick'):
        x, y = map(int, arg.split(',')); out += [at(x, y), 'wait 60', 'buttons %d' % (1 if kind == 'click' else 2), 'wait 60', 'buttons 0', 'wait 60']
    elif kind == 'move':
        x, y = map(int, arg.split(',')); out += [at(x, y), 'wait 30']
    elif kind == 'down':
        x, y = map(int, arg.split(',')); out += [at(x, y), 'wait 60', 'buttons 1', 'wait 60']
    elif kind == 'up': out += ['buttons 0', 'wait 60']
    else: raise SystemExit('unknown item ' + item)
print(';'.join(out))
