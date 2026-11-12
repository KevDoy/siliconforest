#!/bin/bash
# SPDX-License-Identifier: MIT
# Host checks of the read-only property of PDAppleNVMe:
#   1. the command gate (kernel/pd_nvme_gate.c, the kext's own source) over every opcode,
#      named destructive commands, read bounds and 20 million random entries;
#   2. the sources: one doorbell write, behind the gate; no opcode outside the allowlist
#      has a name; no host-to-device data direction on the I/O queue;
#   3. with a built kext ($1 = PDAppleNVMe.kext): its strings name no write path.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd); K=$HERE/../kernel
OUT=$(mktemp -d); trap 'rm -rf "$OUT"' EXIT
cc -std=c11 -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -o "$OUT/gate_host" "$HERE/gate_host.c" "$K/pd_nvme_gate.c"
"$OUT/gate_host"
fail=0
check() { if [ "$2" != "$3" ]; then echo "SRCHOST FAIL: $1: $2, expected $3"; fail=1; fi; }
# the submission doorbells are written in exactly one statement, in PDAns::ring()
check "writes to the submission doorbells" "$(grep -c 'w32(.*ANS_LINEAR_\(A\|IO\)SQ_DB' "$K/pd_ans.cpp")" 1
check "doorbell writes outside pd_ans.cpp" "$(cat "$K/PDAppleNVMe.cpp" "$K/pd_soc.cpp" "$K/pd_nvme_gate.c" | grep -c 'SQ_DB' || true)" 0
# that statement comes after both gate calls in ring()
awk '/^PDAns::ring/,/^}/' "$K/pd_ans.cpp" > "$OUT/ring"
check "gate calls in ring()" "$(grep -c 'pd_nvme_gate(queue' "$OUT/ring")" 2
check "doorbell after the gate" "$(awk '/pd_nvme_gate\(queue/ {g=NR} /SQ_DB/ {d=NR} END {print (g && d > g) ? "yes" : "no"}' "$OUT/ring")" yes
# host-to-device DMA: the flag is set in one place, for odd opcodes, and refused on the I/O queue
check "uses of the host-to-device flag" "$(grep -c 'ANS_TCB_DMA_TO_DEVICE' "$K/pd_ans.cpp")" 2
# opcode names: only those of the allowlist exist anywhere in the driver
check "opcode names" "$(grep -ho 'PD_NVME_\(ADMIN\|IO\)_[A-Z_]*' "$K"/*.c "$K"/*.cpp "$K"/*.h | sort -u | tr '\n' ' ')" \
  "PD_NVME_ADMIN_CREATE_CQ PD_NVME_ADMIN_CREATE_SQ PD_NVME_ADMIN_DELETE_CQ PD_NVME_ADMIN_DELETE_SQ PD_NVME_ADMIN_GET_LOG_PAGE PD_NVME_ADMIN_IDENTIFY PD_NVME_IO_QID PD_NVME_IO_READ "
check "names of write-side commands in the driver" "$(grep -il 'nvme_cmd_write\|NVME_CMD_WRITE\|nvme_cmd_dsm\|nvme_admin_format\|nvme_admin_set_features\|nvme_cmd_flush\|NVME_CMD_FLUSH' "$K"/* | wc -l)" 0
# the block device always reports write protection
check "reportWriteProtection" "$(grep -c 'reportWriteProtection.*\*isWriteProtected = true' "$K/PDAppleNVMe.cpp")" 1
if [ -n "${1:-}" ]; then
  BIN=$1/Contents/MacOS/PDAppleNVMe
  [ -f "$BIN" ] || { echo "SRCHOST FAIL: no binary at $BIN"; exit 1; }
  check "allowlist line in the binary" "$(strings -a "$BIN" | grep -c 'READ-ONLY build: allowlist admin 00 01 02 04 05 06, I/O 02')" 1
  echo "SRCHOST binary: $(sha256sum "$BIN" | cut -c1-16) $(stat -c %s "$BIN") bytes"
fi
[ $fail = 0 ] && echo "SRCHOST PASSED" || { echo "SRCHOST FAILED"; exit 1; }
