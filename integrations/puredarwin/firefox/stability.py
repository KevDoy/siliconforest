# SPDX-License-Identifier: MIT
"""Thirty-minute ordinary-site browsing in this owned browser/profile.

Must run inside the explicitly announced exclusive GUI stability slot.
Content shutdown notifications distinguish abnormal exits from normal process
switching. Network failures remain failures even if the browser survives.
"""
import json
import time

SITES = ['https://example.com/', 'https://en.wikipedia.org/wiki/Firefox',
         'https://developer.mozilla.org/en-US/docs/Web/JavaScript', 'https://www.mozilla.org/en-US/firefox/',
         'https://www.python.org/', 'https://www.rust-lang.org/', 'https://www.gnu.org/',
         'https://www.kernel.org/', 'https://www.w3.org/', 'https://www.rfc-editor.org/rfc/rfc9110.html']


def measure(client, output, deadline, owned_snapshot):
    start = time.monotonic()
    end = min(deadline - 10, start + 1800)
    result = dict(required_seconds=1800, sites=SITES, visits=[], synthetic_navigation=True,
                  crash_observer='ipc:content-shutdown abnormal property', elapsed=0, gpu_identities=[])
    client.command('Marionette:SetContext', {'value': 'chrome'})
    client.command('WebDriver:ExecuteScript', {'script': '''
      const w=Services.wm.getMostRecentWindow("navigator:browser");
      w.__sffxShutdownEvents=[];
      w.__sffxShutdownObserver={observe(subject,topic,data){
        const event={topic,data,wall:Date.now()};
        try{const bag=subject.QueryInterface(Components.interfaces.nsIPropertyBag2);
          event.abnormal=bag.hasKey("abnormal")?bag.getPropertyAsBool("abnormal"):false;}
        catch(error){event.propertyError=String(error);}
        w.__sffxShutdownEvents.push(event);
      }};
      Services.obs.addObserver(w.__sffxShutdownObserver,"ipc:content-shutdown");return true;
      ''', 'args': [], 'newSandbox': False})
    try:
        client.command('Marionette:SetContext', {'value': 'content'})
        handle = client.command('WebDriver:GetWindowHandles')[0]
        client.command('WebDriver:SwitchToWindow', {'handle': handle})
        visit = 0
        while time.monotonic() < end:
            url = SITES[visit % len(SITES)]
            row = dict(requested=url, elapsed=time.monotonic() - start)
            result['visits'].append(row)
            before = time.monotonic()
            previous = client.command('WebDriver:ExecuteScript', {'script':
                'return performance.timeOrigin;', 'args': [], 'newSandbox': False})
            client.command('WebDriver:Navigate', {'url': url})
            until = min(end, before + 15)
            while time.monotonic() < until:
                state = client.command('WebDriver:ExecuteScript', {'script': '''return {
                  url:location.href,documentURI:document.documentURI,title:document.title,
                  timeOrigin:performance.timeOrigin,readyState:document.readyState,textLength:document.body?.innerText.length||0,
                  visible:document.visibilityState};''', 'args': [], 'newSandbox': False})
                row['page'] = state
                if state['readyState'] == 'complete' and state['timeOrigin'] != previous and state['url'] != 'about:blank':
                    break
                time.sleep(.5)
            row['owned_processes'] = owned_snapshot()
            current_gpu = sorted(line.split(None, 5)[0] for line in row['owned_processes'].splitlines()[1:]
                                 if 'gpu-helper.app/Contents/MacOS/Firefox GPU Helper' in line)
            if current_gpu and current_gpu not in result['gpu_identities']:
                result['gpu_identities'].append(current_gpu)
            row['load_seconds'] = time.monotonic() - before
            page = row.get('page', {})
            row['loaded'] = page.get('timeOrigin') != previous and page.get('readyState') == 'complete' and page.get('textLength', 0) > 0 and not page.get('documentURI', '').startswith('about:')
            while time.monotonic() < min(end, before + 18):
                time.sleep(.5)
            result['elapsed'] = time.monotonic() - start
            output.write_text(json.dumps(result, indent=2) + '\n')
            visit += 1
    finally:
        client.command('Marionette:SetContext', {'value': 'chrome'})
        result['shutdown_events'] = client.command('WebDriver:ExecuteScript', {'script': '''
          const w=Services.wm.getMostRecentWindow("navigator:browser");
          Services.obs.removeObserver(w.__sffxShutdownObserver,"ipc:content-shutdown");
          return w.__sffxShutdownEvents;''', 'args': [], 'newSandbox': False})
    result['elapsed'] = time.monotonic() - start
    result['abnormal_content_exits'] = sum(bool(row.get('abnormal')) for row in result['shutdown_events'])
    result['unclassified_content_exits'] = sum('propertyError' in row for row in result['shutdown_events'])
    result['loads_passed'] = sum(row.get('loaded', False) for row in result['visits'])
    result['loads_failed'] = len(result['visits']) - result['loads_passed']
    result['complete'] = result['elapsed'] >= 1800 and result['abnormal_content_exits'] == 0 and result['unclassified_content_exits'] == 0 and len(result['gpu_identities']) == 1 and result['loads_failed'] == 0
    output.write_text(json.dumps(result, indent=2) + '\n')
    return result
