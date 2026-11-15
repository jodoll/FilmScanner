#!/bin/sh
# Host-side initialisation for the sandboxed agent environment.
#
# Applies the host hardening described in docs/13-sandbox.md before the first
# sandbox is created. Run it once per host, from the repository root:
#
#   ./sandbox/init-host.sh
#
# `sbx login` stays a human step: this script never authenticates for you.
# The network allowlist is deliberately not applied here, because every
# destination is a separate decision; see the report for the rules in use.

set -eu

minimum="0.50.0"

if ! command -v sbx >/dev/null 2>&1; then
  echo "sbx is not installed. Install Docker Sandboxes ${minimum} or later." >&2
  exit 1
fi

version=$(sbx version | awk '/^sbx version / { sub(/^v/, "", $3); print $3 }')
if [ -z "$version" ]; then
  echo "Could not read the sbx version." >&2
  exit 1
fi

if ! awk -v have="$version" -v need="$minimum" 'BEGIN {
  split(have, h, ".");
  split(need, n, ".");
  for (i = 1; i <= 3; i++) {
    if ((h[i] + 0) > (n[i] + 0)) exit 0;
    if ((h[i] + 0) < (n[i] + 0)) exit 1;
  }
  exit 0;
}'; then
  echo "sbx ${version} is too old; ${minimum} or later is required." >&2
  exit 1
fi

if ! docker info >/dev/null 2>&1; then
  echo "The Docker daemon is not reachable." >&2
  exit 1
fi

sbx settings set ssh.agentForwardingEnabled false
sbx settings set skills.defaultMode off
sbx settings set telemetry.disabled true
sbx daemon restart

echo "Host settings applied (sbx ${version})."
echo "Next: 'sbx login' once, then build and load the toolchain image."
