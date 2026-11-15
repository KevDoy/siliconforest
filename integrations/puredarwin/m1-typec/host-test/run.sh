#!/bin/bash
# SPDX-License-Identifier: MIT
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
TEMP=$(mktemp -d)
trap 'rm -rf "$TEMP"' EXIT
${CC:-cc} -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
  -I"$HERE/../kernel" "$HERE/test_cd321x.c" -o "$TEMP/test"
"$TEMP/test"
