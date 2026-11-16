#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""FXPERF2 A/B hold: the same page in own Firefox instances with different private framework sets, interleaved,
inside ONE gui.lock hold. Per run: fresh copy of a warm profile, launch with the URL (no Marionette, no tracing),
wait for the window, settle, per-process/thread CPU over <stat> s (fxperf2_m1.py stat, compositor included),
the compositor's WMPACING lines for that window, the parent's loaded images (region map paths), quit.

  fxperf2_ab.py push [<result>:<dir> ...]    copy runs/fxperf2/build/result-<result> to the M1's <prefix>/<dir>
                                             (default true:cand false:base)
  fxperf2_ab.py warm <url>                   make the warm profile template (one launch, 40 s), inside a hold
  fxperf2_ab.py run <tag> <url> <runs> <settle> <stat> <name>=<ENV,...> [<name>=<ENV,...> ...] [capture]
  fxperf2_ab.py grid <tag> <trials> <name>=<ENV,...> [...]  FXLEAD's self-scrolling tile page (served from the prefix's
                                             www on 127.0.0.1:8776), capture when its title says done, every cell checked
  fxperf2_ab.py tabs <tag> <rounds> <name>=<ENV,...> [...]  tab switches (FXPERF's method): 10 static tabs given on the
                                             command line, 12 Command-number switches into the own window per round;
                                             each page posts visibilitychange / 1st / 2nd rAF times (Date.now, the M1's
                                             clock, as the inject request time)
ENV entries KEY=VAL (commas separate; '%' stands for a comma inside a value). 'capture' takes one capture of each
variant's window in its first run (compositor capture, for the visual check); 'sample' takes a 5 s sf-sample of
the parent after each run's stat (profiling run: its CPU numbers are not used)."""
import json, re, subprocess, sys, time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from fxp2 import *

TEMPLATE = B + '/p1-warm'


def compositor_pid():
    return sh("ps -axo pid=,rss=,command= | grep chonkstep-darwin-desktop | grep -v grep | sort -k2 -n | tail -1 | awk '{print $1}'").stdout.strip()


def own_window(before):
    for _ in range(60):
        ws, _t = wins()
        new = [w for w in ws if w['app'] == 'org.mozilla.firefox' and w['mapped'] and w['id'] not in before]
        if new:
            return new[0]
        time.sleep(0.5)
    return None


def quit_own(win):
    try:
        if win is not None:
            inj('target %d\n' % win['id'] + keys('chord:cmd+q', 'wait:300'), timeout=20)
    except Exception as e:
        print('Command-Q not delivered:', e)
    for _ in range(40):
        if not sh(T + ' mine').stdout.strip():
            return ''
        time.sleep(0.5)
    left = sh(T + ' mine').stdout.strip()
    if left:
        print('left after Command-Q, TERM'); sh(T + ' quitall'); time.sleep(2)
    return left


def one(tag, name, env, url, settle, stat, capture, sample=False):
    RO = '%s/out/%s/%s' % (B, tag, name)
    r = dict(variant=name, env=env, url=url)
    before = {w['id'] for w in wins()[0] if w['app'] == 'org.mozilla.firefox'}
    sh('rm -rf {p} && cp -a {t} {p} && mkdir -p {ro}'.format(p=PROFILE, t=TEMPLATE, ro=RO), check=True)
    d0 = int(sh('wc -l < /var/log/desktop-session.log').stdout.split()[0])
    r['launch'] = sh(T + " launch %s '_LAUNCHER=%s/bin/firefox-perf2,%s' %s" % (PROFILE, B, env, url), check=True).stdout.strip()
    r['t_launch'] = time.time()
    win = own_window(before)
    if win is None:
        r['error'] = 'no window'; quit_own(None); return r
    r['win'] = win['line'][:300]; r['t_window'] = time.time()
    time.sleep(settle)
    comp = compositor_pid()
    d1 = int(sh('wc -l < /var/log/desktop-session.log').stdout.split()[0])
    r['stat'] = json.loads(sh(T + ' stat %g %s/stat.json %s | head -1' % (stat, RO, comp)).stdout.strip().splitlines()[0])
    d2 = int(sh('wc -l < /var/log/desktop-session.log').stdout.split()[0])
    st = json.loads(sh('cat %s/stat.json' % RO).stdout)
    for p in st['procs']:
        if p['role'] == 'parent' and p['mine']:
            r['parent_pct'] = p['cpu_pct']
            r['parent_main_ms'] = sum(t['cpu_ms'] for t in p['threads_top'] if t['name'] in ('MainThread', ''))
            r['parent_threads'] = [(t['name'], t['cpu_ms']) for t in p['threads_top'][:8]]
            r['parent_pid'] = p['pid']
        if not p['mine']:
            r['compositor_pct'] = p['cpu_pct']
    r['roles'] = {}
    for p in st['procs']:
        if p['mine']:
            r['roles'][p['role']] = round(r['roles'].get(p['role'], 0) + p['cpu_pct'], 2)
    pac = sh('sed -n "%d,%dp" /var/log/desktop-session.log | grep "WMPACING.*window=%d "' % (d1 + 1, d2 + 12, win['id'])).stdout
    r['wmpacing'] = [l.strip()[:300] for l in pac.splitlines()]
    if 'parent_pid' in r:
        img = sh('python3 %s/tools/fxperf2_m1.py regions %d %s/regions.json >/dev/null; python3 -c "import json;d=json.load(open(\'%s/regions.json\'));'
                 'print(sorted(set(x[\'path\'] for x in d[\'regions\'] if x[\'path\'].startswith((\'/private/var/db/sffxperf2\',\'/System/Library/Frameworks/QuartzCore\',\'/System/Library/Frameworks/OpenGL\',\'/System/Library/Frameworks/AppKit\',\'/usr/lib/libWindowServer\')))))"'
                 % (B, r['parent_pid'], RO, RO), timeout=120).stdout.strip()
        r['images'] = img
    if sample and 'parent_pid' in r:
        # after the clean stat: sf-sample of the parent, 5 s at 2 ms (changes the numbers; never reported as CPU)
        sh('sample %d 5 2 -file %s/sample-parent.txt >/dev/null 2>&1' % (r['parent_pid'], RO), timeout=120)
        r['sampled'] = True
    if capture:
        try:
            inj('target %d\nkey e1 down\nkey e1 up\nwait 400\ncapture fxperf2-%s-%s' % (win['id'], tag, name), timeout=40)
            fetch('fxperf2-%s-%s' % (tag, name), OUT / tag / ('capture-%s.png' % name))
            r['capture'] = 'capture-%s.png' % name
        except Exception as e:
            r['capture_error'] = str(e)[:200]
    r['left'] = quit_own(win)
    pid = r['launch'].split()[0]
    sh('cp $(ls -t /var/root/Library/Logs/SiliconForest/Firefox/*-%s.log | head -1) %s/firefox.log 2>/dev/null; sed -n "%d,\\$p" /var/log/desktop-session.log > %s/desktop.log' % (pid, RO, d0 + 1, RO))
    return r


# FXLEAD's tile checker (runs/fxlead/tools/fxlead.py check_tiles, same page and palette)
GRID = [[230,25,75],[60,180,75],[255,225,25],[0,130,200],[245,130,48],[145,30,180],[70,240,240],[240,50,230],
        [210,245,60],[250,190,212],[0,128,128],[220,190,255],[170,110,40],[255,250,200],[128,0,0],[170,255,195],[0,0,128]]


def check_tiles(path, dpr=1.5, win=None):
    from PIL import Image
    near = lambda a, b, tol=12: all(abs(int(a[i]) - int(b[i])) <= tol for i in range(3))
    im = Image.open(path).convert('RGB'); px = im.load(); w, h = im.size
    o = None
    for y in range(0, h, 3):
        for x in range(0, w, 3):
            if px[x, y] == (255, 0, 255):
                while x > 0 and px[x - 1, y] == (255, 0, 255): x -= 1
                while y > 0 and px[x, y - 1] == (255, 0, 255): y -= 1
                o = (x, y); break
        if o: break
    if not o:
        return dict(error='marker not found')
    ox, oy = o
    bits = ''.join('1' if sum(px[int(ox + (i * 8 + 4) * dpr), int(oy + 28 * dpr)]) < 200 else '0' for i in range(16))
    scroll = int(bits, 2)
    bx = int(ox + 300 * dpr); y = int(oy + 40 * dpr); bottom = None
    while y < im.height - 1:
        if near(px[bx, y], (160, 16, 16), 20):
            bottom = y; break
        y += 1
    right = (win['x'] + win['w']) if win else im.width
    res = dict(origin=o, scrollY=scroll, banner_top=bottom, bad=[], good=0)
    for r in range(400):
        cy = r * 120 - scroll
        y0, y1 = oy + (cy + 3) * dpr, oy + (cy + 30) * dpr
        if y0 < oy + 34 * dpr or bottom is None or y1 >= bottom - 2:
            continue
        for c in range(10):
            x0 = ox + (c * 128 + 3) * dpr
            if x0 + 6 * dpr >= right:
                continue
            exp = GRID[(r * 5 + c) % 17]
            samples = [px[int(x0 + dx), int(y0 + dy)] for dx in (0, 4, 8) for dy in (0, 6, 12, 20)]
            if sum(near(s, exp) for s in samples) >= len(samples) - 1:
                res['good'] += 1
            else:
                res['bad'].append(dict(r=r, c=c, x=int(x0 - ox), y=int(y0 - oy), got=samples[0], exp=exp))
    if bottom is not None:
        res['banner_bad_x'] = [x - ox for x in range(ox + 4, right - 4, 40) if not near(px[x, bottom + int(5 * dpr)], (160, 16, 16), 20)]
    return res


def grid_one(tag, name, env):
    r = dict(variant=name, env=env)
    before = {w['id'] for w in wins()[0] if w['app'] == 'org.mozilla.firefox'}
    sh('rm -rf {p} && cp -a {t} {p}'.format(p=PROFILE, t=TEMPLATE), check=True)
    r['launch'] = sh(T + " launch %s '_LAUNCHER=%s/bin/firefox-perf2,%s' http://127.0.0.1:8776/tiles.html" % (PROFILE, B, env), check=True).stdout.strip()
    win = None
    t0 = time.time()
    while time.time() - t0 < 40:
        ws, _t = wins()
        new = [w for w in ws if w['app'] == 'org.mozilla.firefox' and w['mapped'] and w['id'] not in before]
        if new and 'tiles-done' in new[0]['title']:
            win = new[0]; break
        time.sleep(1)
    if win is None:
        r['error'] = 'no done window'; r['left'] = quit_own(None); return r
    time.sleep(1.5)
    try:
        inj('target %d\nkey e1 down\nkey e1 up\nwait 400\ncapture fxperf2-%s-%s' % (win['id'], tag, name), timeout=40)
        cap = fetch('fxperf2-%s-%s' % (tag, name), OUT / tag / ('grid-%s.png' % name))
        c = check_tiles(cap, win=win)
        r['check'] = dict(scrollY=c.get('scrollY'), good=c.get('good'), bad=len(c.get('bad', [])), banner_bad=len(c.get('banner_bad_x', [])),
                          error=c.get('error'), first_bad=c.get('bad', [])[:4])
    except Exception as e:
        r['capture_error'] = str(e)[:200]
    r['left'] = quit_own(win)
    return r


TAB_HTML = '''<!doctype html><meta charset=utf-8><title>FXPERF tab</title>
<style>body{font:20px sans-serif;margin:40px}</style>
<h1 id=h>FXPERF tab</h1><p>Static page for tab-switch timing.</p>
<script>
const i=new URLSearchParams(location.search).get('i')||'0';document.title='FXPERF tab '+i;
document.body.style.background=['#fff','#eef','#efe','#fee','#ffe','#eff','#fef','#ddd','#cdf','#fdc'][i%10];
function post(n,o){fetch('/r/'+n,{method:'POST',body:JSON.stringify(o)}).catch(()=>{});}
document.addEventListener('visibilitychange',()=>{if(document.visibilityState!=='visible')return;const t0=Date.now(),p0=performance.now();
 requestAnimationFrame(()=>{const t1=Date.now();requestAnimationFrame(()=>{post('tab'+i,{i,vis:t0,raf1:t1,raf2:Date.now(),tori:performance.timeOrigin,p0});});});});
</script>'''
SERVE = '''import http.server, time, sys
from pathlib import Path
W = Path(sys.argv[1]); R = Path(sys.argv[2]); R.mkdir(parents=True, exist_ok=True)
class H(http.server.SimpleHTTPRequestHandler):
    def __init__(s, *a, **k): super().__init__(*a, directory=str(W), **k)
    def do_POST(s):
        n = int(s.headers.get('Content-Length', 0)); b = s.rfile.read(n)
        name = ''.join(c for c in s.path.rsplit('/', 1)[-1] if c.isalnum() or c in '-_') or 'r'
        (R / ('%s-%d.json' % (name, int(time.time() * 1000)))).write_bytes(b)
        s.send_response(204); s.end_headers()
    def log_message(s, *a): pass
http.server.ThreadingHTTPServer(('127.0.0.1', 8776), H).serve_forever()
'''
ORDER = [2, 3, 4, 5, 1, 6, 7, 8, 2, 3, 9, 4]


def tabs_one(tag, name, env, resdir):
    r = dict(variant=name, env=env)
    before = {w['id'] for w in wins()[0] if w['app'] == 'org.mozilla.firefox'}
    sh('rm -rf {p} && cp -a {t} {p} && rm -rf {r} && mkdir -p {r}'.format(p=PROFILE, t=TEMPLATE, r=resdir), check=True)
    urls = ' '.join("'http://127.0.0.1:8776/tab.html?i=%d'" % i for i in range(10))
    r['launch'] = sh(T + " launch %s '_LAUNCHER=%s/bin/firefox-perf2,%s' %s" % (PROFILE, B, env, urls), check=True).stdout.strip()
    win = own_window(before)
    if win is None:
        r['error'] = 'no window'; quit_own(None); return r
    time.sleep(8)
    sw = []
    for k in ORDER:
        q = sh(T + ' req 10', inp='target %d\n' % win['id'] + keys('chord:cmd+%d' % k, 'wait:300'), timeout=40)
        m = re.search(r't_req=(\d+)', q.stdout)
        sw.append(dict(k=k, t_req=int(m[1]) if m else None, rc=q.returncode))
        time.sleep(0.9)
    time.sleep(1)
    res = []
    for line in sh('for f in %s/*.json; do echo "$(basename $f) $(cat $f)"; done' % resdir).stdout.splitlines():
        try:
            res.append(json.loads(line.split(' ', 1)[1]))
        except Exception:
            pass
    out = []
    for s_ in sw:
        if not s_['t_req']:
            continue
        want = str(s_['k'] - 1) if s_['k'] < 9 else '9'
        c = [x for x in res if x['i'] == want and s_['t_req'] <= x['vis'] <= s_['t_req'] + 3000]
        if c:
            out.append(dict(k=s_['k'], vis=c[0]['vis'] - s_['t_req'], raf1=c[0]['raf1'] - s_['t_req'], raf2=c[0]['raf2'] - s_['t_req']))
    r['switches'] = out
    r['left'] = quit_own(win)
    return r


def main():
    a = sys.argv[1:]
    if a[0] == 'push':
        want = a[1:] or ['true:cand', 'false:base']
        for src, dst in (x.split(':') for x in want):
            res = REPO / ('runs/fxperf2/build/result-' + src)
            tar = subprocess.run(['tar', '-C', str(res.resolve()), '-cf', '-', '.'], capture_output=True, check=True).stdout
            r = subprocess.run(SSH + ['rm -rf %s/%s && mkdir -p %s/%s && tar -C %s/%s -xf - && chmod -R u+w %s/%s && mkdir -p %s/%s/Frameworks/AppKit.framework/Versions/C && ln -sf ../A/AppKit %s/%s/Frameworks/AppKit.framework/Versions/C/AppKit && shasum -a 256 %s/%s/lib/* %s/%s/Frameworks/*.framework/Versions/A/* 2>/dev/null | cut -c1-12,65-'
                                    % ((B, dst) * 8)], input=tar, capture_output=True)
            print(dst, r.returncode, r.stdout.decode()[-800:], r.stderr.decode()[-300:])
        return
    if a[0] == 'warm':
        with hold(2, 'warm profile template for own Firefox (no input)'):
            sh('rm -rf %s %s && mkdir -p %s && cat > %s/user.js' % (PROFILE, TEMPLATE, PROFILE, PROFILE), inp=USERJS, check=True)
            before = {w['id'] for w in wins()[0] if w['app'] == 'org.mozilla.firefox'}
            print(sh(T + " launch %s '_LAUNCHER=%s/bin/firefox-perf2' %s" % (PROFILE, B, a[1]), check=True).stdout)
            win = own_window(before)
            time.sleep(40)
            print('left', quit_own(win))
            sh('cp -a %s %s' % (PROFILE, TEMPLATE), check=True)
        return
    if a[0] == 'tabs':
        import statistics
        tag, rounds = a[1], int(a[2])
        variants = [x.split('=', 1) for x in a[3:] if '=' in x]
        (OUT / tag).mkdir(parents=True, exist_ok=True)
        sh('mkdir -p %s/www && cat > %s/www/tab.html' % (B, B), inp=TAB_HTML, check=True)
        sh('cat > %s/tools/serve.py' % B, inp=SERVE, check=True)
        res = []
        with hold(max(1, round(rounds * len(variants) * 35 / 60)), 'FXPERF2 tabs %s: own Firefox, 10 static tabs, 12 Command-number switches into own window per round, %d x %s' % (
                tag, rounds, '/'.join(n for n, _ in variants))):
            srv = shbg('exec python3 %s/tools/serve.py %s/www %s/results' % (B, B, B), OUT / tag / 'http.log')
            time.sleep(1.5)
            try:
                for i in range(rounds):
                    for name, env in variants:
                        r = tabs_one(tag, '%s-%d' % (name, i), env, B + '/results'); r['i'] = i; r['name'] = name; res.append(r)
                        v = sorted(x['raf2'] for x in r.get('switches', []))
                        print(now(), name, i, 'n=%d raf2 p50=%s' % (len(v), statistics.median(v) if v else '-'), r.get('error', ''), flush=True)
                        json.dump(res, open(OUT / tag / 'tabs.json', 'w'), indent=1)
            finally:
                if sh(T + ' mine').stdout.strip():
                    sh(T + ' quitall')
                sh("pkill -f 'sffxperf2/tools/serve.py' || true")
                srv.terminate()
        for name, _env in variants:
            for k in ('vis', 'raf1', 'raf2'):
                v = sorted(x[k] for r in res if r['name'] == name for x in r.get('switches', []))
                if v:
                    print('%s %s: n=%d p50=%d p95=%d min=%d max=%d ms' % (name, k, len(v), statistics.median(v), v[min(len(v) - 1, int(.95 * len(v)))], v[0], v[-1]))
        return
    if a[0] == 'grid':
        tag, trials = a[1], int(a[2])
        variants = [x.split('=', 1) for x in a[3:] if '=' in x]
        (OUT / tag).mkdir(parents=True, exist_ok=True)
        res = []
        with hold(max(1, round(trials * len(variants) * 18 / 60)), 'FXPERF2 grid %s: own Firefox on the self-scrolling tile page, %d x %s, Shift tap + capture of own window' % (
                tag, trials, '/'.join(n for n, _ in variants))):
            sh('cat > %s/tools/serve.py' % B, inp=SERVE, check=True)
            srv = shbg('exec python3 %s/tools/serve.py %s/www %s/results' % (B, B, B), OUT / tag / 'http.log')
            time.sleep(1.5)
            try:
                for i in range(trials):
                    for name, env in variants:
                        r = grid_one(tag, '%s-%d' % (name, i), env); r['i'] = i; r['name'] = name; res.append(r)
                        print(now(), name, i, json.dumps(r.get('check') or r.get('error') or r.get('capture_error'))[:300], flush=True)
                        json.dump(res, open(OUT / tag / 'grid.json', 'w'), indent=1)
            finally:
                if sh(T + ' mine').stdout.strip():
                    sh(T + ' quitall')
                sh("pkill -f 'sffxperf2/tools/serve.py' || true")
                srv.terminate()
        for name, _env in variants:
            rows = [r for r in res if r['name'] == name]
            wrong = [r for r in rows if not r.get('check') or r['check'].get('error') or r['check']['bad'] or r['check']['banner_bad']]
            print('%s: %d trials, %d with wrong cells/banner or no check' % (name, len(rows), len(wrong)))
        return
    if a[0] == 'run':
        tag, url, runs, settle, stat = a[1], a[2], int(a[3]), float(a[4]), float(a[5])
        variants = [x.split('=', 1) for x in a[6:] if '=' in x]
        capture = 'capture' in a[6:]
        sample = 'sample' in a[6:]
        (OUT / tag).mkdir(parents=True, exist_ok=True)
        res = []
        what = 'FXPERF2 A/B %s: %s, %d x %s (own Firefox, no input but Command-Q/capture to own window)' % (
            tag, url, runs, '/'.join(n for n, _ in variants))
        est = max(1, round(runs * len(variants) * (settle + stat + 25) / 60))
        with hold(est, what):
            try:
                for i in range(runs):
                    for name, env in variants:
                        print(now(), 'run', i, name, flush=True)
                        r = one(tag, '%s-%d' % (name, i), env, url, settle, stat, capture and i == 0, sample)
                        r['i'] = i; r['name'] = name; res.append(r)
                        print(json.dumps({k: r.get(k) for k in ('name', 'i', 'parent_pct', 'parent_main_ms', 'roles', 'wmpacing', 'error')})[:900], flush=True)
                        json.dump(res, open(OUT / tag / 'ab.json', 'w'), indent=1)
            finally:
                if sh(T + ' mine').stdout.strip():
                    print('cleanup TERM'); sh(T + ' quitall')
        r = subprocess.run(SSH + ['tar -C %s/out -cf - %s' % (B, tag)], capture_output=True, timeout=900)
        subprocess.run(['tar', '-C', str(OUT), '-xf', '-'], input=r.stdout, check=True)
        return
    raise SystemExit(__doc__)


if __name__ == '__main__':
    main()
