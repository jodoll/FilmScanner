#!/bin/sh
# Build the sandbox images and load the project image as the sbx template.
#
# Run from anywhere; the script uses the repository root as the Docker build
# context because both Dockerfiles read project files.
#
#   ./sandbox/build-image.sh
#
# The tar is a temporary transport file: it is written for `sbx template load`
# and deleted after the import. Rebuild when the toolchain or platformio.ini
# changes, then recreate the sandbox to use the new image.

set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo_root"

tar="$repo_root/sandbox/film-scanner-tools.tar"

if ! command -v docker >/dev/null 2>&1; then
  echo "docker is not installed or not on PATH." >&2
  exit 1
fi
if ! docker info >/dev/null 2>&1; then
  echo "The Docker daemon is not reachable." >&2
  exit 1
fi
if ! command -v sbx >/dev/null 2>&1; then
  echo "sbx is not installed or not on PATH." >&2
  exit 1
fi

docker build \
  -f "$repo_root/sandbox/toolchain.Dockerfile" \
  -t film-scanner-toolchain:v1 \
  .

docker build \
  -f "$repo_root/sandbox/tools.Dockerfile" \
  -t film-scanner-tools:v1 \
  .

docker image save film-scanner-tools:v1 -o "$tar"
sbx template load "$tar"

rm -f "$tar"

echo "Built and loaded film-scanner-tools:v1."
