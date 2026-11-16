# SPDX-License-Identifier: MIT
"""Repeat the installed Firefox hamburger regression through the virtual seat.

Caller owns the browser session and gui.lock. Every click targets the uniquely
admitted browser window; actual trusted button events and native window survival
are checked separately from Gecko's popup state. This is functional QA only.
"""
import json
from pathlib import Path
import re
import secrets
import time


def measure(client, output, deadline, admission=None):
    from presentation_seat import admit, _request
    from renderer_capture import capture
    inject = Path('/private/var/run/gpu-desktop/inject')
    result = dict(complete=False, synthetic_input=True, virtual_seat=True,
                  requested_repetitions=20, repetitions=[], timing_qualified=False)

    def save():
        output.write_text(json.dumps(result, indent=2) + '\n')

    def chrome(source):
        client.command('Marionette:SetContext', {'value': 'chrome'})
        return client.command('WebDriver:ExecuteScript', dict(script=source, args=[], newSandbox=False))

    def request(lines):
        if time.monotonic() > deadline - 10:
            raise RuntimeError('Popup stress deadline')
        return _request(inject, lines)

    def state():
        return chrome('''const w=Services.wm.getMostRecentWindow("navigator:browser");
          if(!w)return {windows:0};
          return {windows:[...Services.wm.getEnumerator("navigator:browser")].length,
            panel:w.document.getElementById("appMenu-popup")?.state,
            events:w.__sffxPopupEvents??[],tabs:w.gBrowser.tabs.length};''')

    try:
        if admission is None:
            admission = admit(client, output.with_name('popup-admission.json'), fixture=False)
        result['admission'] = admission
        name = 'sffx-popup-frame-' + secrets.token_hex(5)
        request('dump ' + name)
        dump = (inject / (name + '.txt')).read_text()
        output.with_name('popup-current-frame.txt').write_text(dump)
        native = [line for line in dump.splitlines()
                  if re.match(r'window %d ' % admission['window_id'], line) and 'mapped=true' in line]
        frame = re.search(r'frame=Some\((.*?)\)', native[0]) if len(native) == 1 else None
        values = list(map(int, re.findall(r'-?\d+', frame[1]))) if frame else []
        if len(values) != 4:
            raise RuntimeError('Admitted owned popup-test window is no longer mapped')
        result['current_frame'] = values
        result['before'] = state()
        if result['before']['windows'] != 1:
            raise RuntimeError('Expected one owned browser window')
        anchors = chrome('''const w=Services.wm.getMostRecentWindow("navigator:browser");
          const b=w.document.getElementById("PanelUI-menu-button");
          if(!b)throw new Error("No application menu button");
          w.__sffxPopupEvents=[];
          b.addEventListener("mousedown",e=>w.__sffxPopupEvents.push({trusted:e.isTrusted,button:e.button}),true);
          const anchors=[];
          for(const [index,color] of [[253,1,251],[1,253,173]].entries()){
            const e=w.document.createElementNS("http://www.w3.org/1999/xhtml","div");
            e.id="sffx-popup-anchor-"+index;
            e.style.cssText=`all:initial;position:fixed;left:${64+32*index}px;top:${32+32*index}px;width:8px;height:8px;display:block;background:rgb(${color.join(',')});z-index:2147483647;pointer-events:none`;
            w.document.documentElement.appendChild(e);const r=e.getBoundingClientRect();
            anchors.push({x:r.x,y:r.y,width:r.width,height:r.height,color});
          }
          return {anchors,scale:w.devicePixelRatio};''')
        time.sleep(.3)
        calibration_path = output.with_name('popup-chrome-calibration.ppm')
        result['chrome_capture'] = capture(calibration_path)
        from screen_calibration import calibrate
        result['chrome_calibration'] = calibration = calibrate(calibration_path, anchors['anchors'], anchors['scale'])
        chrome('''const w=Services.wm.getMostRecentWindow("navigator:browser");
          w.document.querySelectorAll('[id^="sffx-popup-anchor-"]').forEach(e=>e.remove());return true;''')
        save()
        for index in range(20):
            point = chrome('''const w=Services.wm.getMostRecentWindow("navigator:browser");
              const r=w.document.getElementById("PanelUI-menu-button").getBoundingClientRect();
              return {x:r.x+r.width/2,y:r.y+r.height/2};''')
            point = {axis: calibration['origin'][i] + point[axis]*calibration['scale']
                     for i, axis in enumerate(['x', 'y'])}
            x, y, width, height = result['current_frame']
            px, py = point['x'], point['y']
            if not (0 <= px <= 2559 and 0 <= py <= 1599 and x <= px < x+width and y <= py < y+height):
                raise RuntimeError('Application menu button lies outside the admitted owned frame')
            row = dict(attempt=index+1, point=point, complete=False)
            result['repetitions'].append(row)
            save()
            request('abs %d %d\nbuttons 1\nwait 50\nbuttons 0\nwait 250' %
                    (round(px*65535/2559), round(py*65535/1599)))
            row['opened'] = state()
            events = row['opened'].get('events', [])
            if (row['opened'].get('panel') != 'open' or len(events) != index+1 or
                    not all(event['trusted'] and event['button'] == 0 for event in events)):
                raise RuntimeError('Application menu did not open after the trusted virtual-seat click')
            if index in [0, 4, 9, 14, 19]:
                row['capture'] = capture(output.with_name('popup-%02d-renderer.ppm' % (index+1)))
            request('key 29 down\nkey 29 up\nwait 150')
            row['closed'] = state()
            name = 'sffx-popup-' + secrets.token_hex(5)
            request('dump ' + name)
            dump = (inject / (name + '.txt')).read_text()
            output.with_name('popup-%02d-windows.txt' % (index+1)).write_text(dump)
            mapped = [line for line in dump.splitlines()
                      if re.match(r'window %d ' % admission['window_id'], line) and 'mapped=true' in line]
            row['complete'] = bool(len(mapped) == 1 and row['closed'].get('windows') == 1 and
                                   row['closed'].get('panel') == 'closed')
            save()
            if not row['complete']:
                raise RuntimeError('Application menu close lost the browser window or left a popup open')
        result['after'] = state()
        result['complete'] = len(result['repetitions']) == 20 and all(row['complete'] for row in result['repetitions'])
    finally:
        save()
    return result
