#!/bin/sh
# Start the film-scanner sandbox, creating it if necessary.
#
#   ./.sandbox/start.sh          # create or reattach
#   ./.sandbox/start.sh --reset  # remove the sandbox, then create it fresh
#
# The images are built only when the sandbox template is missing. An outdated
# template is the operator's responsibility: run ./.sandbox/build-image.sh,
# then ./.sandbox/start.sh --reset.
#
# Run ./.sandbox/init-host.sh once on a new host before the first start.

set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo_root"

reset=0
case "${1:-}" in
  "") ;;
  --reset) reset=1 ;;
  *) echo "usage: $0 [--reset]" >&2; exit 2 ;;
esac

if ! command -v sbx >/dev/null 2>&1; then
  echo "sbx is not installed or not on PATH." >&2
  exit 1
fi

if ! sbx template ls -q | grep -qF 'film-scanner-tools:v1'; then
  ./.sandbox/build-image.sh
fi

if [ "$reset" -eq 1 ]; then
  sbx rm film-scanner
fi

if sbx ls | grep -q '^film-scanner'; then
  sbx run --name film-scanner
else
  sbx run docker.io/sbx/opencode:2.3.0 \
    --name film-scanner \
    --clone \
    -t film-scanner-tools:v1 \
    --kit ./.sandbox/opencode-state \
    .
fi

