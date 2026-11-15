# SPDX-License-Identifier: MIT
"""Focus an admitted owned fixture and prove a live renderer before timing."""
import json
import os
from pathlib import Path
import re
import secrets
import time


def admit(client, output):
    def script(source, args=None):
        return client.command('WebDriver:ExecuteScript', dict(script=source, args=args or [], newSandbox=False))

    client.command('Marionette:SetContext', {'value': 'content'})
    for handle in client.command('WebDriver:GetWindowHandles'):
        client.command('WebDriver:SwitchToWindow', {'handle': handle})
        if script('return typeof fxMeasureFrames === "function";'):
            break
    else:
        raise RuntimeError('Owned presentation fixture absent')
    marker = 'SF-FX presentation-' + secrets.token_hex(8)
    client.command('WebDriver:ExecuteAsyncScript', dict(script='''
      const marker=arguments[0],done=arguments[arguments.length-1];
      fxMeasureFrames(250).then(()=>{document.title=marker;done(true);});''', args=[marker], newSandbox=False))
    inject = Path('/private/var/run/gpu-desktop/inject')

    def request(lines):
        if (inject / 'request').exists() or (inject / 'request.part').exists():
            raise RuntimeError('Another inject request is pending')
        (inject / 'done').unlink(missing_ok=True)
        with (inject / 'request.part').open('x') as stream:
            stream.write(lines + '\n')
        os.replace(inject / 'request.part', inject / 'request')
        end = time.monotonic()+6
        while not (inject / 'done').exists() and time.monotonic() < end:
            time.sleep(.05)
        status = (inject / 'done').read_text()
        if not re.search(r'errors=0\b', status):
            raise RuntimeError(status)

    name = 'sffx-presentation-' + secrets.token_hex(5)
    request('dump ' + name)
    dump = (inject / (name + '.txt')).read_text()
    output.with_suffix('.windows.txt').write_text(dump)
    matches = [line for line in dump.splitlines() if marker in line and 'mapped=true' in line and 'org.mozilla.firefox' in line]
    if len(matches) != 1:
        raise RuntimeError('Expected exactly one uniquely titled owned fixture frame')
    frame = re.search(r'frame=Some\((.*?)\)', matches[0])
    values = tuple(map(int, re.findall(r'-?\d+', frame[1]))) if frame else ()
    if len(values) != 4:
        raise RuntimeError('Owned frame geometry absent')
    x, y, width, height = values
    tx, ty = x+width//2, y+10
    if not (width>100 and height>100 and 0<=tx<=2559 and 0<=ty<=1599):
        raise RuntimeError('Owned titlebar outside the admitted panel')
    # Pointer motion wakes a sleeping display; the following click belongs to
    # the admitted window. No desktop power/idle configuration is changed.
    request('abs %d %d\nwait 500\nbuttons 1\nwait 60\nbuttons 0\nwait 300' %
            (round(tx*65535/2559), round(ty*65535/1599)))
    client.command('Marionette:SetContext', {'value': 'chrome'})
    focused = script('const w=Services.wm.getMostRecentWindow("navigator:browser");return !!w&&w.document.hasFocus();')
    if not focused:
        raise RuntimeError('Admitted Firefox did not acquire focus')
    from renderer_capture import capture
    renderer = capture(output.with_name('presentation-before.ppm'))
    client.command('Marionette:SetContext', {'value': 'content'})
    result = dict(complete=True, synthetic_seat_input=True, admitted_unique_title=marker,
                  frame=values, focused=focused, renderer=renderer)
    output.write_text(json.dumps(result, indent=2)+'\n')
    return result
