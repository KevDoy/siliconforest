#!/bin/bash
# Build kext + KC, assemble a packet, deliver it to the M5, boot it once on the M1,
# return to Linux and fetch the console. Usage: tools/pcie_iterate.sh <candidate-number> [runtime]
set -euo pipefail
N=${1:?candidate number}; RUNTIME=${2:-120}
export PATH=$HOME/.nix-profile/bin:$PATH
ROOT=$(cd "$(dirname "$0")/.." && pwd); cd "$ROOT"
SRC=integrations/puredarwin/m1-apcie-host; RUN=runs/m1-desktop-010
NAME=pcie-enum-candidate-$N; W=/Users/chriskearney/darwin-lab-firstboot-007; M5=chriskearney@10.1.1.93
NIX="nix-build --extra-experimental-features nix-command --extra-experimental-features flakes"
if [ "${PCIE_SKIP_BUILD:-0}" = 1 ]; then
  echo "reusing the existing kernel collection and tools"
elif [ "${PCIE_FROM_PATCH:-0}" = 1 ]; then
  # Build everything from the PureDarwin tree patches instead of the lab directories.
  tools/pcie_export_patch.sh
  P=integrations/puredarwin/puredarwin-patches
  mkdir -p $RUN/bwfm-kc-build-001 $RUN/pd-dhcp-build-001
  (cd $P && $NIX build-kc.nix -o "$ROOT/$RUN/bwfm-kc-build-001/result" > "$ROOT/$RUN/bwfm-kc-build-001/build.log" 2>&1) || { grep -n ' error: ' -A4 $RUN/bwfm-kc-build-001/build.log | cut -c1-240 | head -40; exit 1; }
  (cd $P && $NIX build-userspace-from-patch.nix -o "$ROOT/$RUN/pd-dhcp-build-001/result" > "$ROOT/$RUN/pd-dhcp-build-001/build.log" 2>&1) || { tail -20 $RUN/pd-dhcp-build-001/build.log; exit 1; }
else
mkdir -p $RUN/apcie-host-build-001 $RUN/apcie-host-kc-build-001
(cd $SRC && $NIX build-kext.nix -o "$ROOT/$RUN/apcie-host-build-001/result" > "$ROOT/$RUN/apcie-host-build-001/build.log" 2>&1) || { grep -n 'error' -A6 $RUN/apcie-host-build-001/build.log | head -40; exit 1; }
K=$(readlink -f $RUN/apcie-host-build-001/result)
sed -i "s|bridge = /nix/store/[^;]*;|bridge = $K/System/Library/Extensions/PDApcieT8103Bridge.kext;|" $SRC/build-kc.nix
(cd $SRC && $NIX build-kc.nix -o "$ROOT/$RUN/apcie-host-kc-build-001/result" > "$ROOT/$RUN/apcie-host-kc-build-001/build.log" 2>&1) || { tail -30 $RUN/apcie-host-kc-build-001/build.log; exit 1; }
if [ "${PCIE_WIFI:-0}" = 1 ]; then
  mkdir -p $RUN/pd-netd-build-001
  (cd integrations/puredarwin/m1-bwfm/netd && $NIX build-pd-netd.nix -o "$ROOT/$RUN/pd-netd-build-001/result" > "$ROOT/$RUN/pd-netd-build-001/build.log" 2>&1) || { grep -n ' error: ' -A4 $RUN/pd-netd-build-001/build.log | head -20; exit 1; }
  mkdir -p $RUN/bwfm-kc-build-001
  (cd integrations/puredarwin/m1-bwfm && $NIX build-kc.nix -o "$ROOT/$RUN/bwfm-kc-build-001/result" > "$ROOT/$RUN/bwfm-kc-build-001/build.log" 2>&1) || { grep -n ' error: ' -A4 $RUN/bwfm-kc-build-001/build.log | cut -c1-240 | head -40; exit 1; }
fi
fi
python3 -B tools/pcie_assemble.py $NAME | tail -2
LAB=$RUN/pcie-m2-host-001/lab
cp integrations/puredarwin/m1-apcie-bridge/proxy_linkup.py $LAB/integrations/puredarwin/m1-bringup/pcie_linkup.py
echo "${PCIE_LINK:-none}" > $LAB/integrations/puredarwin/m1-bringup/pcie_linkup_mode.txt
echo "${PCIE_CPUFREQ:-on}" > $LAB/integrations/puredarwin/m1-bringup/cpufreq_mode.txt
LSHA=$(sha256sum $LAB/integrations/puredarwin/m1-bringup/host_progress.py | cut -d' ' -f1)
tar -C $RUN/pcie-m2-host-001 -cf - lab | ssh -o BatchMode=yes $M5 "sudo -n sh -c 'rm -rf $W/lab-pcie-m2-001 && mkdir -m 700 $W/lab-pcie-m2-001 && tar -xf - -C $W/lab-pcie-m2-001 --strip-components 1 && chown -R root:staff $W/lab-pcie-m2-001'"
tar -C artifacts/m1-desktop-010 -cf - $NAME/manifest.json $NAME/build-record.json $NAME/source.patch $NAME/kernel.macho $NAME/ramdisk.img $NAME/ramdisk.record.json | ssh -o BatchMode=yes $M5 "sudo -n sh -c 'mkdir -p -m 700 $W/pcie-m2 && rm -rf $W/pcie-m2/$NAME && tar -xf - -C $W/pcie-m2 && chown -R root:staff $W/pcie-m2'"
echo "delivered $NAME; booting"
ssh -o BatchMode=yes -o ServerAliveInterval=15 $M5 "sudo -n sh -c 'cd $W/lab-pcie-m2-001 && exec $W/venv/bin/python -B tools/m1_bringup_cycle.py --execute --confirm-recovery J293_RECOVERY_READY --cycles 1 --candidate $W/pcie-m2/$NAME/manifest.json --launcher-sha256 $LSHA --inspector-sha256 ea36952b5d68118583f03bdf756683a4a83b534fd4d1a96a62a23e2e54a8b062 --runtime $RUNTIME --inventory $W/lab/reports/sprint-007/ssh-target-inventory.json --private-root $W/pcie-m2-cycles'" | tail -2 || true
R=$RUN/pcie-enum-attempt-$N; mkdir -p $R
ssh -o BatchMode=yes $M5 "sudo -n sh -c 'cd $W/pcie-m2-cycles && d=\$(ls -td cycle-*/001 | head -1) && tar -cf - -C \$d guest/console.bin guest/worker.log result.json'" | tar -xf - -C $R
grep -a 'PCIE_LINKUP\|CPUFREQ' $R/guest/worker.log || true
strings -n 6 $R/guest/console.bin | grep -E 'PDApcieT8103|PDBCM4378|PDBwfm|PDDart|DL3[2-9]|WMBENCH|WMSTAT|PDRust|pd-dhcp|pd-netd|PD_DHCP|ping|bytes from|HTTP/|netsetup|Connected|Couldn|wifiutil|SSID|interface:|power:|address:|state:|panic\(|IOPCI' | cut -c1-330 | head -${LINES_MAX:-80} || true
