#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
${CC:-cc} -std=c11 -O1 -g -fsanitize=address,undefined -I "$here/../include" -c "$here/../src/pd_atc_usb2.c" -o "$tmp/atc.o"
${CXX:-c++} -std=c++14 -O1 -g -fsanitize=address,undefined -I "$here/../include" "$here/atc.cpp" "$tmp/atc.o" -o "$tmp/atc"
"$tmp/atc"
