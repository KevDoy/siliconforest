"""Assemble the PCIe enumeration packet (M2). Offline; no transport or hardware."""
from pathlib import Path
import datetime,hashlib,json,shutil,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'artifacts/m1-desktop-010/apcie-port-candidate-003'
NAME=sys.argv[1] if len(sys.argv)>1 else 'pcie-enum-candidate-001'
OUT=ROOT/'artifacts/m1-desktop-010'/NAME
LAB=ROOT/'runs/m1-desktop-010/pcie-m2-host-001/lab'
import os
WIFI=os.environ.get('PCIE_WIFI')=='1'
KERNEL=(ROOT/('runs/m1-desktop-010/bwfm-kc-build-001/result/kernel' if WIFI else 'runs/m1-desktop-010/apcie-host-kc-build-001/result/kernel')).resolve()
SUPERVISOR=ROOT/('integrations/puredarwin/m1-bwfm/supervisor/darwin-wifi-test' if WIFI else 'integrations/puredarwin/m1-apcie-host/supervisor/darwin-pcie-enum-test')
STAGER=ROOT/'integrations/puredarwin/chonkstep-darwin/stage_ramdisk.py'
ORIGINAL=ROOT/'artifacts/m1-bringup-009/ramdisk.img'
SRC=ROOT/'integrations/puredarwin/m1-apcie-host'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
load=lambda p:json.loads(Path(p).read_text())
def save(p,d):
 with Path(p).open('x')as f:json.dump(d,f,indent=2);f.write('\n')
OUT.mkdir(mode=0o700)
JOIN=os.environ.get('PCIE_WIFI_JOIN')=='1'
if JOIN:
 env=dict(l.strip().split('=',1) for l in (ROOT/'private/wifi.env').read_text().splitlines() if '=' in l)
 assert "'" not in env['WIFI_SSID']+env['WIFI_PSK']
 filled=OUT/'darwin-wifi-join-test.private'
 filled.write_text((ROOT/'integrations/puredarwin/m1-bwfm/supervisor/darwin-wifi-join-test.in').read_text().replace('@SSID@',env['WIFI_SSID']).replace('@PSK@',env['WIFI_PSK']))
 filled.chmod(0o700)
 SUPERVISOR=filled
mapping=[]
for e in load(BASE/'mapping.json')['files']:
 e=dict(e)
 if e['destination']=='/usr/bin/darwin-desktop-test':e['source']=str(SUPERVISOR)
 mapping.append(e)
if JOIN:
 mapping.append({'source':str((ROOT/'runs/m1-desktop-010/pd-dhcp-build-001/result/usr/bin/pd-dhcp').resolve()),'destination':'/usr/bin/pd-dhcp','executable':True})
if WIFI:
 # This M1's own firmware, private and never committed.
 for f in sorted((ROOT/'private/firmware/brcm').iterdir()):
  mapping.append({'source':str(f),'destination':'/Library/Firmware/brcm/'+f.name,'executable':False})
save(OUT/'mapping.json',{'files':mapping})
with(OUT/'stage.log').open('x')as f:subprocess.run([sys.executable,'-B',str(STAGER),'--base',str(ORIGINAL),'--mapping',str(OUT/'mapping.json'),'--output',str(OUT/'ramdisk.img')],stdout=f,stderr=subprocess.STDOUT,check=True)
ram=load(OUT/'ramdisk.record.json');assert ram['e2fsck_exit_code']==0
shutil.copyfile(KERNEL,OUT/'kernel.macho')
text=(BASE/'source.patch').read_text()
if WIFI:
 B=ROOT/'integrations/puredarwin/m1-bwfm'
 for q in sorted(B.iterdir()):
  if q.is_file():text+='\n# COMPONENT PDBwfm (ISC bwfm port; GPL-2.0 as a whole with the DART pool)\n# FILE '+str(q.relative_to(ROOT))+' sha256='+sha(q)+'\n'+q.read_text(errors='replace')+'\n'
for n in ['CMakeLists.txt','Info.plist','NOTICE','PDApcieT8103.cpp','PDApcieT8103.h','dart/pd_dart_pool.cpp','dart/pd_dart_pool.h','build-kext.nix','build-kc.nix','supervisor/darwin-pcie-enum-test']:
 p=SRC/n;text+='\n# COMPONENT PCIe host bridge (GPL-2.0)\n# FILE '+str(p.relative_to(ROOT))+' sha256='+sha(p)+'\n'+p.read_text()+'\n'
(OUT/'source.patch').write_text(text)
record=load(BASE/'build-record.json')
record.update(recorded_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),source_patch_sha256=sha(OUT/'source.patch'),output_path=str(KERNEL.parent),kernel_sha256=sha(KERNEL),
 hardware_gate='PCIE_WIFI_TRACK_NO_ADMISSION_REQUIRED',desktop_test_mode='PCIE_HOST_BRIDGE_ENUMERATION',
 source_patch_scope='native50 source envelope plus the GPL-2.0 PDApcieT8103Bridge host bridge; launcher brings the link up from the proxy',
 not_claimed=['MSI','DART/DMA','BCM4378 firmware or networking'])
record['host_input_sources']={k:sha(LAB/k) for k in record['host_input_sources']}
record['ramdisk']={'sha256':sha(OUT/'ramdisk.img'),'payload_record_sha256':sha(OUT/'ramdisk.record.json'),'guest_script_sha256':sha(SUPERVISOR),'scope':'candidate-003 payloads with the supervisor replaced'}
save(OUT/'build-record.json',record)
manifest=load(BASE/'manifest.json')
manifest['artifacts']={k:{'path':n,'sha256':sha(OUT/n),'bytes':(OUT/n).stat().st_size}for k,n in [('kernel','kernel.macho'),('source_patch','source.patch'),('build_record','build-record.json'),('ramdisk','ramdisk.img')]}
save(OUT/'manifest.json',manifest)
code='''import sys,json\nfrom pathlib import Path\nfrom types import SimpleNamespace\nlab,p=map(Path,sys.argv[1:]);sys.path[:0]=[str(lab),str(lab/'integrations/puredarwin/m1-bringup')]\nimport host_progress as h,normal_image as n\nreport,contents=n.verify_manifest(p/'manifest.json');i=SimpleNamespace(report=report,contents=contents)\nassert h.apcie_port_observer_enabled(i) is True;h.apcie_port_runtime(i,120,240)\nargs=h.guest_bootargs(i);assert args.endswith(' pdapciebridge=1'),args\nprint(json.dumps({'bootargs':args,'members':len(report['kernel']['members'])}))\n'''
x=subprocess.run([sys.executable,'-B','-c',code,str(LAB),str(OUT)],capture_output=True,text=True,timeout=120,cwd=LAB)
print(x.stdout[-600:],x.stderr[-3000:]);x.check_returncode()
print(OUT)
