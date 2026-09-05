#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export CC="${CC:-clang-23}"
export CXX="${CXX:-clang++-23}"

case "$(basename -- "$CC")" in
    clang|clang-[0-9]*) ;;
    *)
        printf 'OpenLegend Linux builds require Clang; CC=%s\n' "$CC" >&2
        exit 2
        ;;
esac
case "$(basename -- "$CXX")" in
    clang++|clang++-[0-9]*) ;;
    *)
        printf 'OpenLegend Linux builds require Clang; CXX=%s\n' "$CXX" >&2
        exit 2
        ;;
esac

exec python3 "$ROOT/tools/build.py" "$@"
