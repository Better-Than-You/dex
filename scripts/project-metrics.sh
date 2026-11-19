#!/usr/bin/env bash
# Print reproducible project metrics. Used by CI and by the README's Metrics table.
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)

count_lines() {
    find "$1" -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.qml' \) \
        -print0 | xargs -0 cat | wc -l
}

version=$(tr -d '[:space:]' < "$root/VERSION")
core=$(count_lines "$root/src/core")
engine=$(count_lines "$root/src/engine")
provider=$(count_lines "$root/src/provider")
qml_lines=$(count_lines "$root/qml")
tests=$(count_lines "$root/tests")
src_files=$(find "$root/src" -type f \( -name '*.cpp' -o -name '*.hpp' \) | wc -l)
suites=$(find "$root/tests" -name 'tst_*.cpp' | wc -l)

printf 'version=%s\n' "$version"
printf 'src_files=%s\n' "$src_files"
printf 'src_loc=%s\n' "$((core + engine + provider))"
printf 'core_loc=%s\n' "$core"
printf 'engine_loc=%s\n' "$engine"
printf 'provider_loc=%s\n' "$provider"
printf 'qml_loc=%s\n' "$qml_lines"
printf 'test_suites=%s\n' "$suites"
printf 'test_loc=%s\n' "$tests"
printf 'total_loc=%s\n' "$((core + engine + provider + qml_lines + tests))"
