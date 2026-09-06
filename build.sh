#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export CC="${CC:-clang-23}"
export CXX="${CXX:-clang++-23}"

exec python3 "$ROOT/build.py" "$@"
