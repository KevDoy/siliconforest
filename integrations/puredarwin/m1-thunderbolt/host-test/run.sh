#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
${CC:-cc} -std=gnu11 -O1 -g -fsanitize=address,undefined \
  -I "$here/upstream" -I "$here/portable" \
  "$here/upstream/apple-dpin-state.c" "$here/src/pd_dpin_policy.c" \
  "$here/upstream/apple-dpin-test.c" -o "$tmp/dpin"
"$tmp/dpin"
${CC:-cc} -std=c11 -O1 -g -fsanitize=address,undefined -I "$here/include" -c "$here/src/pd_apple_nhi.c" -o "$tmp/nhi.o"
${CXX:-c++} -std=c++14 -O1 -g -fsanitize=address,undefined -I "$here/include" "$here/host-test/nhi.cpp" "$tmp/nhi.o" -o "$tmp/nhi"
"$tmp/nhi"
${CC:-cc} -std=c11 -O1 -g -fsanitize=address,undefined -I "$here/include" -c "$here/src/pd_cio_lifecycle.c" -o "$tmp/cio.o"
${CXX:-c++} -std=c++14 -O1 -g -fsanitize=address,undefined -I "$here/include" "$here/host-test/cio.cpp" "$tmp/cio.o" -o "$tmp/cio"
"$tmp/cio"
${CC:-cc} -std=c11 -O1 -g -fsanitize=address,undefined -I "$here/include" -c "$here/src/pd_cio_tunables.c" -o "$tmp/tunables.o"
${CXX:-c++} -std=c++14 -O1 -g -fsanitize=address,undefined -I "$here/include" "$here/host-test/tunables.cpp" "$tmp/tunables.o" -o "$tmp/tunables"
"$tmp/tunables"
${CC:-cc} -std=c11 -O1 -g -fsanitize=address,undefined -I "$here/include" "$here/host-test/wire.c" -o "$tmp/wire"
"$tmp/wire"
${CC:-cc} -std=c11 -O1 -g -fsanitize=address,undefined -I "$here/include" -c "$here/src/pd_tb_registry.c" -o "$tmp/registry.o"
${CXX:-c++} -std=c++14 -O1 -g -pthread -fsanitize=address,undefined -I "$here/include" "$here/host-test/registry.cpp" "$tmp/registry.o" -o "$tmp/registry"
"$tmp/registry"
${CC:-cc} -std=c11 -O1 -g -fsanitize=address,undefined \
  -I "$here/host-test/device-linux" -I "$here/native" -I "$here/nativeinc" -I "$here/pininc" -I "$here/include" \
  "$here/native/pd_tb_device.c" "$here/src/pd_tb_registry.c" "$here/host-test/device.c" -pthread -o "$tmp/device"
"$tmp/device"
"$tmp/device" fault
