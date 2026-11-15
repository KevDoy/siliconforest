#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
${CXX:-c++} -std=c++14 -O1 -g -fsanitize=address,undefined -pthread \
    -I "$here/../kernel" "$here/lifetime.cpp" -o "$tmp/lifetime"
"$tmp/lifetime"
