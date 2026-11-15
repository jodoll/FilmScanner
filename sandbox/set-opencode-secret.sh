#!/bin/sh
# Store the OpenCode API key as a global sbx custom secret.
#
#   ./sandbox/set-opencode-secret.sh
#
# The secret is keyed to the OpenCode hosts and the OPENCODE_API_KEY
# environment variable. The sandbox receives only a placeholder; the
# host-side proxy replaces it with the real key in request headers on
# the way out.
#
# Global by design: available to every sandbox. Custom secrets are
# picked up when a sandbox is created, so recreate afterwards:
#
#   ./sandbox/start.sh --reset
#
# To update an existing token, rerun this script; recreate the sandbox
# if the regenerated placeholder differs.

set -eu

if ! command -v sbx >/dev/null 2>&1; then
  echo "sbx is not installed or not on PATH." >&2
  exit 1
fi

sbx secret set-custom \
  --host opencode.ai \
  --host '*.opencode.ai' \
  --env OPENCODE_API_KEY

echo "Stored. Verify with: sbx secret ls --host opencode.ai"
