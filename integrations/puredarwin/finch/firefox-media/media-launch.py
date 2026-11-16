#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Own-profile media comparison using the installed runtime's graphics plan.

No installed selector, owner profile, graphics framework or broker is changed.
Only the explicitly packaged media frameworks precede the selected providers.
"""
import importlib.util
import json
import os
from pathlib import Path
import sys


def main():
    root=Path(__file__).resolve().parent
    if os.uname().sysname!='Darwin' or not root.is_relative_to(Path('/private/var/db/sffx')):
        raise ValueError('Only a cold private SiliconForest packet may launch')
    spec=importlib.util.spec_from_file_location('media_daily',root/'tools/daily-launch.py')
    daily=importlib.util.module_from_spec(spec);spec.loader.exec_module(daily)
    profile=daily.selected_profile(sys.argv[1:])
    if profile is None or not profile.is_relative_to(root) or '--no-remote' not in sys.argv:
        raise ValueError('Explicit owned no-remote profile required')
    frameworks=root/'Frameworks'
    names={p.name for p in frameworks.iterdir()}
    if not names or names-{'CoreVideo.framework','VideoToolbox.framework','AudioToolbox.framework'}:
        raise ValueError('Only reviewed media frameworks may be selected')
    runtime=Path('/usr/local/lib/siliconforest/firefox/160-system1')
    command,env=daily.plan(runtime,sys.argv[1:])
    env['DYLD_FRAMEWORK_PATH']=str(frameworks)+':'+env['DYLD_FRAMEWORK_PATH']
    (profile.parent/'private-media-launch.json').write_text(json.dumps(dict(
        command=command,frameworks=sorted(names),installed_outer_launcher=False,
        installed_runtime_plan=True,owner_profile=False,
        environment={k:v for k,v in env.items() if k.startswith(('DYLD_','SF_VT_','SF_AU_'))}),indent=2)+'\n')
    os.execvpe(command[0],command,env)


if __name__=='__main__':main()
