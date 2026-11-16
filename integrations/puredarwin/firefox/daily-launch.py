#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Launch the untouched Firefox bundle with this private release's providers."""
import json
import os
from pathlib import Path
import socket
import sys
import time


def selected_profile(args):
    for index,arg in enumerate(args):
        if arg in ['--profile','-profile'] and index+1<len(args):
            return Path(args[index+1]).expanduser().resolve()
        if arg.startswith('--profile='):
            return Path(arg.partition('=')[2]).expanduser().resolve()
    return None


def plan(root, args, manifest=None):
    args = list(args)
    # Preserve the caller's interpretation before changing to the bundle's
    # directory. A relative profile must never end up in the original bundle.
    for index, arg in enumerate(args):
        if arg in ['--profile', '-profile'] and index + 1 < len(args):
            args[index + 1] = str(Path(args[index + 1]).expanduser().resolve())
        elif arg.startswith('--profile='):
            args[index] = '--profile=' + str(Path(arg.partition('=')[2]).expanduser().resolve())
    if manifest is None:
        manifest = json.loads((root / 'release.json').read_text())
    diagnostic = manifest.get('diagnostics', {})
    if diagnostic:
        allowed = {'PD_WS_TRACE','PD_WS_TIMING','PD_CA_TRACE','PD_SANDBOX_TRACE','MOZ_REMOTE_ALLOW_SYSTEM_ACCESS'}
        if set(diagnostic.get('environment',{}))-allowed or diagnostic.get('args',[]) not in [[],['--marionette']]:
            raise ValueError('Unsupported temporary installation diagnostics')
        args = diagnostic.get('args',[]) + args
    if str(root) != manifest['target']:
        raise RuntimeError('Runtime moved: rebuild its CGL/sandbox paths for the new prefix')
    system_ui = manifest.get('system_ui', False)
    if type(system_ui) is not bool or (system_ui and not manifest.get('system_layout')):
        raise ValueError('System UI selection requires an installed system runtime')
    system_gpu = manifest.get('system_gpu_providers', False)
    if type(system_gpu) is not bool or (system_gpu and not system_ui):
        raise ValueError('System GPU providers require the canonical system UI cohort')
    env = os.environ.copy()
    env.update(diagnostic.get('environment',{}))
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
    surface_framework = root / 'surface/Frameworks'
    if manifest.get('surface_override') is not None:
        surface_framework = Path(manifest['surface_override'])
        if (not manifest.get('system_layout') or '..' in surface_framework.parts or
                not surface_framework.is_relative_to(root / 'overrides') or surface_framework.name != 'Frameworks'):
            raise ValueError('IOSurface override must be a versioned provider inside this app runtime')
        frames[0] = surface_framework
    onyx = manifest.get('onyx_override')
    if onyx is not None:
        requested = Path(onyx)
        if (not manifest.get('system_layout') or '..' in requested.parts or
                not requested.is_relative_to(root / 'overrides') or requested.name != 'Frameworks'):
            raise ValueError('Onyx override must be a versioned provider inside this app runtime')
        frames[4] = requested
    ui = manifest.get('ui_override')
    if ui is not None:
        ui = Path(ui)
        if (not manifest.get('system_layout') or '..' in ui.parts or
                not ui.is_relative_to(root / 'overrides') or ui == root / 'overrides'):
            raise ValueError('UI override must be a versioned provider inside this app runtime')
        frames[2] = ui / 'appkit/Frameworks'
    coretext_source = 'system' if system_ui else manifest.get('coretext_source', 'private')
    if coretext_source not in ['private', 'system']:
        raise ValueError('Unknown CoreText selection')
    if manifest.get('private_fonts', False):
        font_framework = root / 'coretext/Frameworks'
        override = manifest.get('coretext_override')
        if override is not None:
            requested = Path(override)
            if (not manifest.get('system_layout') or '..' in requested.parts or
                    not requested.is_relative_to(root / 'overrides') or requested.name != 'Frameworks'):
                raise ValueError('CoreText override must be a versioned provider inside this app runtime')
            font_framework = requested
        if coretext_source == 'private':
            frames.insert(0, font_framework)
        env['FONTCONFIG_FILE'] = str(root / 'fonts/fonts.conf')
    libraries = [root / 'avfoundation/Frameworks/AVFoundation.framework/Versions/A',
        root / 'quartz/Frameworks/QuartzCore.framework/Versions/A', root / 'gl/lib', root / 'compat/usr/lib']
    if manifest.get('gpu_presentation', False):
        libraries.insert(0, (ui or root) / 'windowserver/usr/lib')
    if system_ui:
        # The missing-provider tree contains no AppKit, Onyx2D, CoreText or
        # QuartzCore. Remove private library directories too: QuartzCore.pd
        # and libWindowServer must follow the selected system frameworks.
        frames = [surface_framework, root / 'avfoundation/Frameworks', root / 'gl/Frameworks']
        frames += [root / 'compat/System/Library/Frameworks', Path('/System/Library/Frameworks')]
        libraries = [root / 'avfoundation/Frameworks/AVFoundation.framework/Versions/A',
                     root / 'gl/lib', root / 'compat/usr/lib']
        if system_gpu:
            frames = [root / 'avfoundation/Frameworks', root / 'compat/System/Library/Frameworks',
                      Path('/System/Library/Frameworks')]
            libraries = [root / 'avfoundation/Frameworks/AVFoundation.framework/Versions/A', root / 'compat/usr/lib']
    env.update(DYLD_FRAMEWORK_PATH=':'.join(map(str, frames)),
        DYLD_LIBRARY_PATH=':'.join(map(str, libraries)) + ':/usr/local/lib/mesa-asahi',
        DYLD_INSERT_LIBRARIES=str(root / 'sandbox/libsffx-sandbox.dylib') + ':' + str(root / 'io/libsffx-io-policy.dylib'),
        XDG_RUNTIME_DIR='/private/var/run/gpu-desktop',
        WAYLAND_DISPLAY='wayland-0', SFFX_IO_POLICY='default',
        SFFX_IOSURFACE_SERVICE='org.puredarwin.iosurfaced.sffx.' + str(os.getpid()))
    observer = env.get('SFFX_DIAGNOSTIC_OBSERVER')
    if observer:
        observers = observer.split(':')
        explicit_profile = selected_profile(args)
        private = Path('/private/var/db/sffx')
        if (not 1 <= len(observers) <= 4 or explicit_profile is None or
                not explicit_profile.is_relative_to(private)):
            raise ValueError('Diagnostic audio observer requires an explicit isolated Firefox profile')
        for name in observers:
            requested = Path(name)
            if (not name or not requested.is_absolute() or '..' in requested.parts or
                    not requested.is_relative_to(private) or requested.suffix != '.dylib'):
                raise ValueError('Every diagnostic observer must be inside the private test prefix')
        env['DYLD_INSERT_LIBRARIES'] += ':' + ':'.join(observers)
    for name in list(env):
        if name.startswith('MOZ_DISABLE_') and name.endswith('_SANDBOX'):
            raise RuntimeError('Refusing inherited disabled sandbox: ' + name)
    original = Path(manifest.get('original_bundle', str(root / 'original/Firefox.app')))
    broker = Path('/usr/libexec/iosurfaced') if system_gpu else root / 'surface/bin/iosurfaced'
    command = [str(root / 'surface/bin/surface-session'), str(broker),
               str(original / 'Contents/MacOS/firefox')]
    if selected_profile(args) is None:
        command += ['--profile', str(default_profile(root, manifest))]
    command += args
    return command, env


def default_profile(root, manifest):
    if manifest.get('system_layout'):
        return Path.home() / 'Library/Application Support/Firefox/Profiles/siliconforest'
    return root / 'profile'


def main():
    root = Path(__file__).resolve().parents[3]
    config = Path(__file__).with_name('system-runtime.json')
    if config.exists():
        root = Path(json.loads(config.read_text())['target'])
        if root.parent != Path('/usr/local/lib/siliconforest/firefox') or root.is_symlink():
            raise RuntimeError('Unexpected installed runtime path')
    arguments = sys.argv[1:]
    dry = arguments[:1] == ['--sf-plan']
    command, env = plan(root, arguments[1:] if dry else arguments)
    if dry:
        print(json.dumps(dict(command=command, environment={k: v for k, v in env.items()
            if k.startswith(('DYLD_', 'SFFX_'))}, sandbox_enabled=True), indent=2))
        return
    if os.uname().sysname != 'Darwin':
        raise RuntimeError('Runtime requires SiliconForest')
    # A stale inherited SSH environment or an absent desktop must not produce
    # a long connection retry followed by an unreachable, headless parent.
    display = Path(env['XDG_RUNTIME_DIR']) / env['WAYLAND_DISPLAY']
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as probe:
            probe.settimeout(1)
            probe.connect(str(display))
    except OSError as error:
        raise RuntimeError('Firefox desktop is unavailable: ' + str(display)) from error
    manifest = json.loads((root / 'release.json').read_text())
    profile = selected_profile(arguments) or default_profile(root, manifest)
    profile.mkdir(mode=0o700, parents=True, exist_ok=True)
    preferences = {'browser.shell.checkDefaultBrowser': False, 'app.update.auto': False,
                   'webgl.disable-angle': True, 'browser.tabs.inTitlebar': 0}
    legacy = ''.join('user_pref(%s,%s);\n' % (json.dumps(k),json.dumps(v)) for k,v in preferences.items())
    preferences.update({'browser.aboutwelcome.enabled': False, 'browser.preonboarding.enabled': False,
                        'browser.startup.homepage_override.mstone': 'ignore',
                        'startup.homepage_welcome_url': '', 'startup.homepage_welcome_url.additional': '',
                        'datareporting.policy.dataSubmissionEnabled': False, 'toolkit.telemetry.enabled': False})
    settings = profile / 'user.js'
    # Upgrade only the byte-exact four-setting file this installer generated.
    # An existing owner-edited user.js remains untouched.
    if not settings.exists() or (manifest.get('system_layout') and settings.read_text() == legacy):
        settings.write_text(''.join('user_pref(%s,%s);\n' % (json.dumps(k), json.dumps(v))
                                   for k, v in preferences.items()))
    logs = (Path.home() / 'Library/Logs/SiliconForest/Firefox') if manifest.get('system_layout') else root / 'logs'
    logs.mkdir(mode=0o700, parents=True, exist_ok=True)
    fd = os.open(logs / ('firefox-%d-%d.log' % (time.time(), os.getpid())), os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    os.dup2(fd, 1)
    os.dup2(fd, 2)
    os.close(fd)
    os.chdir(Path(manifest.get('original_bundle', str(root / 'original/Firefox.app'))) / 'Contents/MacOS')
    os.execve(command[0], command, env)


if __name__ == '__main__':
    main()
