#!/bin/bash
# SPDX-License-Identifier: MIT
# Host checks of the key strip logic and picture. Output directory: $1 (default: a temporary one).
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-$(mktemp -d)}
cc -std=c11 -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -o "$OUT/tb_host" "$HERE/tb_host.c" "$HERE/../kernel/pd_tb_keys.c"
"$OUT/tb_host" "$OUT"
echo "pictures in $OUT"
