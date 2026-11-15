# SPDX-License-Identifier: MIT
"""Focus an admitted owned fixture and prove a live renderer before timing."""
import json
import os
from pathlib import Path
import re
import secrets
import time


def _request(inject, lines):
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


def admit(client, output, fixture=True, verify_renderer=True):
    def script(source, args=None):
        return client.command('WebDriver:ExecuteScript', dict(script=source, args=args or [], newSandbox=False))

    client.command('Marionette:SetContext', {'value': 'content'})
    for handle in client.command('WebDriver:GetWindowHandles'):
        client.command('WebDriver:SwitchToWindow', {'handle': handle})
        if not fixture or script('return typeof fxMeasureFrames === "function";'):
            break
    else:
        raise RuntimeError('Owned presentation fixture absent')
    marker = 'SF-FX presentation-' + secrets.token_hex(8)
    previous = script('''return {title:document.title,video:typeof fxVideoResult==="object",
      override:window.fxTitleOverride??null};''')
    if fixture:
        client.command('WebDriver:ExecuteAsyncScript', dict(script='''
          const marker=arguments[0],done=arguments[arguments.length-1];
          fxMeasureFrames(250).then(()=>{document.title=marker;done(true);});''', args=[marker], newSandbox=False))
    else:
        script('if(arguments[1])window.fxTitleOverride=arguments[0];document.title=arguments[0];return true;',
               [marker, previous['video']])
        time.sleep(.25)
    inject = Path('/private/var/run/gpu-desktop/inject')

    def request(lines):
        _request(inject, lines)

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
    renderer = capture(output.with_suffix('.ppm')) if verify_renderer else None
    client.command('Marionette:SetContext', {'value': 'content'})
    script('''document.title=arguments[0];if(arguments[1]){
      if(arguments[2]===null)delete window.fxTitleOverride;else window.fxTitleOverride=arguments[2];}return true;''',
           [previous['title'], previous['video'], previous['override']])
    result = dict(complete=True, synthetic_seat_input=True, admitted_unique_title=marker,
                  frame=values, window_id=int(re.match(r"window (\d+) ", matches[0])[1]),
                  focused=focused, renderer=renderer)
    output.write_text(json.dumps(result, indent=2)+'\n')
    return result


def pulse(admission, output):
    """Wake via pointer motion only after fresh own-process/frame/stack checks."""
    import subprocess
    admission = Path(admission)
    if not str(admission).startswith('/private/var/db/sffx/') or '..' in admission.parts:
        raise ValueError('Admission must be in the owned private prefix')
    data = json.loads(admission.read_text())
    providers = json.loads(admission.with_name('providers.json').read_text())
    parents = [row['pid'] for row in providers['processes'] if row['role'] == 'parent']
    if len(parents) != 1:
        raise RuntimeError('Owned benchmark parent absent')
    command = subprocess.run(['ps','-p',str(parents[0]),'-o','command='],capture_output=True,text=True).stdout
    if str(admission.parent/'profile') not in command:
        raise RuntimeError('Owned benchmark parent identity no longer matches its profile')
    window = data.get('window_id')
    if window is None:
        old = admission.with_suffix('.windows.txt').read_text()
        matches = [line for line in old.splitlines() if data['admitted_unique_title'] in line and 'org.mozilla.firefox' in line]
        if len(matches) != 1:
            raise RuntimeError('Original unique window admission absent')
        window = int(re.match(r'window (\d+) ', matches[0])[1])
    inject = Path('/private/var/run/gpu-desktop/inject')
    name = 'sffx-bench-pulse-'+secrets.token_hex(5)
    _request(inject, 'dump '+name)
    dump = (inject/(name+'.txt')).read_text()
    lines = [line for line in dump.splitlines() if line.startswith('window %d ' % window)
             and 'org.mozilla.firefox' in line and 'mapped=true' in line]
    stack = re.search(r'^stack \[(.*?)\]$',dump,re.MULTILINE)
    top = [int(value) for value in re.findall(r'\d+',stack[1])] if stack else []
    if len(lines) != 1 or not top or top[-1] != window:
        raise RuntimeError('Owned benchmark window is no longer the top mapped window')
    frame = re.search(r'frame=Some\((.*?)\)',lines[0])
    values = tuple(map(int,re.findall(r'-?\d+',frame[1]))) if frame else ()
    if len(values) != 4:
        raise RuntimeError('Owned benchmark frame absent')
    x,y,width,height = values;px=x+width//2;py=max(2,y+10)
    if not (width>100 and height>100 and x<=px-2<px<x+width and y<=py<y+height
            and 2<=px<=2559 and 0<=py<=1599):
        raise RuntimeError('Owned benchmark title region outside panel')
    def point(a):return round(a*65535/2559)
    _request(inject,'abs %d %d\nwait 60\nabs %d %d' %
             (point(px-2),round(py*65535/1599),point(px),round(py*65535/1599)))
    result = dict(complete=True,synthetic_pointer_only=True,parent=parents[0],window=window,frame=values)
    Path(output).write_text(json.dumps(result,indent=2)+'\n')
    return result
