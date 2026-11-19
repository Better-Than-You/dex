#!/usr/bin/env bash
# Usage: scripts/bump-version.sh 1.2.3
# Updates VERSION and inserts a dated CHANGELOG section under [Unreleased].
set -euo pipefail

new=${1:?Usage: bump-version.sh X.Y.Z}
if [[ ! "$new" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]+)?$ ]]; then
    printf 'Version must follow Semantic Versioning (X.Y.Z), got: %s\n' "$new" >&2
    exit 1
fi

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
old=$(tr -d '[:space:]' < "$root/VERSION")
if [[ "$new" == "$old" ]]; then
    printf 'Version is already %s\n' "$new" >&2
    exit 1
fi

printf '%s\n' "$new" > "$root/VERSION"

tmp=$(mktemp)
awk -v ver="$new" -v day="$(date -u +%F)" '
    /^## \[Unreleased\]/ && !inserted {
        print
        print ""
        print "## [" ver "] - " day
        print ""
        print "### Changed"
        print ""
        print "- _Describe the release here._"
        inserted = 1
        next
    }
    { print }
' "$root/CHANGELOG.md" > "$tmp"
mv -- "$tmp" "$root/CHANGELOG.md"

printf 'Bumped %s -> %s. Review CHANGELOG.md, commit, then tag v%s.\n' "$old" "$new" "$new"
