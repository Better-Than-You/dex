#!/usr/bin/env bash
# Validate version metadata and print the canonical version.
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
version=$(tr -d '[:space:]' < "$root/VERSION")

if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]+)?$ ]]; then
    printf 'Invalid version in VERSION: %s\n' "$version" >&2
    exit 1
fi

if ! grep -Fq "## [$version]" "$root/CHANGELOG.md"; then
    printf 'CHANGELOG.md has no section for version %s\n' "$version" >&2
    exit 1
fi

printf '%s\n' "$version"
