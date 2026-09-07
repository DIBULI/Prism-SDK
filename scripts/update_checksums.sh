#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
manifest_tmp=$(mktemp)
trap 'rm -f "$manifest_tmp"' EXIT
while IFS= read -r -d '' published_file; do
  [ "$published_file" = SHA256SUMS ] && continue
  [ -f "$published_file" ] || continue
  sha256sum "$published_file" >> "$manifest_tmp"
done < <(git ls-files -c -o --exclude-standard -z | LC_ALL=C sort -zu)
mv "$manifest_tmp" SHA256SUMS
trap - EXIT
