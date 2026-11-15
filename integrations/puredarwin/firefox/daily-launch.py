#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Launch the untouched Firefox bundle with this private release's providers."""
import json
import os
from pathlib import Path
import sys
import time


def selected_profile(args):
    for index,arg in enumerate(args):
        if arg in ['--profile','-profile'] and index+1<len(args):
            return Path(args[index+1]).expanduser().resolve()
        if arg.startswith('--profile='):
            return Path(arg.partition('=')[2]).expanduser().resolve()
    return None


def plan(root, args):
    args = list(args)
    # Preserve the caller's interpretation before changing to the bundle's
    # directory. A relative profile must never end up in the original bundle.
    for index, arg in enumerate(args):
        if arg in ['--profile', '-profile'] and index + 1 < len(args):
            args[index + 1] = str(Path(args[index + 1]).expanduser().resolve())
        elif arg.startswith('--profile='):
            args[index] = '--profile=' + str(Path(arg.partition('=')[2]).expanduser().resolve())
    manifest = json.loads((root / 'release.json').read_text())
    if str(root) != manifest['target']:
        raise RuntimeError('Runtime moved: rebuild its CGL/sandbox paths for the new prefix')
    env = os.environ.copy()
    for field, name in [('gpu_presentation', 'SFFX_GPU_PRESENT'), ('gpu_presentation', 'SFFX_IOSURFACE_GPU'),
                        ('gpu_vector_compare', 'SFFX_GPU_VECTOR_COMPARE'), ('gpu_cache_lookups', 'SFFX_GPU_CACHE_LOOKUPS'),
                        ('gpu_dirty_frames', 'SFFX_GPU_DIRTY_FRAMES'), ('gpu_export_cache', 'SFFX_GPU_EXPORT_CACHE'),
                        ('gpu_coalesce_ticks', 'SFFX_GPU_COALESCE_TICKS'),
                        ('precise_scroll', 'SFFX_PRECISE_SCROLL')]:
        if manifest.get(field, False):
            env[name] = '1'
        else:
            env.pop(name, None)
    frames = [root / (name + '/Frameworks') for name in
              ['surface', 'avfoundation', 'appkit', 'quartz', 'onyx', 'gl']]
    frames.append(root / 'compat/System/Library/Frameworks')
    if manifest.get('private_fonts', False):
        frames.insert(0, root / 'coretext/Frameworks')
        env['FONTCONFIG_FILE'] = str(root / 'fonts/fonts.conf')
    libraries = [root / 'avfoundation/Frameworks/AVFoundation.framework/Versions/A',
        root / 'quartz/Frameworks/QuartzCore.framework/Versions/A', root / 'gl/lib', root / 'compat/usr/lib']
    if manifest.get('gpu_presentation', False):
        libraries.insert(0, root / 'windowserver/usr/lib')
    env.update(DYLD_FRAMEWORK_PATH=':'.join(map(str, frames)),
        DYLD_LIBRARY_PATH=':'.join(map(str, libraries)) + ':/usr/local/lib/mesa-asahi',
        DYLD_INSERT_LIBRARIES=str(root / 'sandbox/libsffx-sandbox.dylib') + ':' + str(root / 'io/libsffx-io-policy.dylib'),
        XDG_RUNTIME_DIR=env.get('XDG_RUNTIME_DIR', '/private/var/run/gpu-desktop'),
        WAYLAND_DISPLAY=env.get('WAYLAND_DISPLAY', 'wayland-0'), SFFX_IO_POLICY='default',
        SFFX_IOSURFACE_SERVICE='org.puredarwin.iosurfaced.sffx.' + str(os.getpid()))
    for name in list(env):
        if name.startswith('MOZ_DISABLE_') and name.endswith('_SANDBOX'):
            raise RuntimeError('Refusing inherited disabled sandbox: ' + name)
    command = [str(root / 'surface/bin/surface-session'), str(root / 'surface/bin/iosurfaced'),
               str(root / 'original/Firefox.app/Contents/MacOS/firefox')]
    if selected_profile(args) is None:
        command += ['--profile', str(root / 'profile')]
    command += args
    return command, env


def main():
    root = Path(__file__).resolve().parents[3]
    arguments = sys.argv[1:]
    dry = arguments[:1] == ['--sf-plan']
    command, env = plan(root, arguments[1:] if dry else arguments)
    if dry:
        print(json.dumps(dict(command=command, environment={k: v for k, v in env.items()
            if k.startswith(('DYLD_', 'SFFX_'))}, sandbox_enabled=True), indent=2))
        return
    if os.uname().sysname != 'Darwin':
        raise RuntimeError('Runtime requires SiliconForest')
    profile = selected_profile(arguments) or root / 'profile'
    profile.mkdir(mode=0o700, exist_ok=True)
    preferences = {'browser.shell.checkDefaultBrowser': False, 'app.update.auto': False,
                   'webgl.disable-angle': True, 'browser.tabs.inTitlebar': 0}
    settings = profile / 'user.js'
    if not settings.exists():
        settings.write_text(''.join('user_pref(%s,%s);\n' % (json.dumps(k), json.dumps(v))
                                   for k, v in preferences.items()))
    logs = root / 'logs'
    logs.mkdir(mode=0o700, exist_ok=True)
    fd = os.open(logs / ('firefox-%d-%d.log' % (time.time(), os.getpid())), os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    os.dup2(fd, 1)
    os.dup2(fd, 2)
    os.close(fd)
    os.chdir(root / 'original/Firefox.app/Contents/MacOS')
    os.execve(command[0], command, env)


if __name__ == '__main__':
    main()
