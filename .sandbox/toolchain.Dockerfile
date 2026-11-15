# Base image for the film-scanner sandbox.
#
# Build from the repository root, for the host architecture:
#   docker build -f .sandbox/toolchain.Dockerfile -t film-scanner-toolchain:v1 .
#
# It carries the toolchains and the OpenCode 2 runtime. The project image in
# tools.Dockerfile builds on it and adds the project libraries.

# Dated tag, not the moving :opencode tag. This tag resolves to
# sha256:7feb9784bc882dbe014888becb65c23e7fb1d1b24169a68d33660fe44476a0b4
# (Ubuntu 26.04, agent uid 1000), the same image the tag pointed at on build.
FROM docker/sandbox-templates:opencode-0.11.0

# Root is used only for apt, the global npm removal and the system-wide
# PlatformIO install. Everything else runs as agent.
USER root

RUN export DEBIAN_FRONTEND=noninteractive \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential \
      ca-certificates \
      curl \
      pipx \
      python3 \
      python3-venv \
 && rm -rf /var/lib/apt/lists/*

# The sandbox template ships OpenCode 1.19.5 through npm. Remove it so it
# cannot shadow OpenCode 2 on PATH.
RUN npm uninstall -g opencode-ai || true \
 && rm -rf /usr/local/share/npm-global/lib/node_modules/opencode-ai \
           /usr/local/share/npm-global/bin/opencode

# Install PlatformIO system-wide, not in the agent's home, so the toolchain
# does not depend on the state volume. The command stays on PATH for the agent.
RUN PIPX_HOME=/opt/pipx \
    PIPX_BIN_DIR=/usr/local/bin \
    pipx install platformio==6.2.0

USER agent

# Install OpenCode 2 with its own installer. The v1 installer refuses 2.x;
# this is the dedicated v2 path. The binary lands in
# /home/agent/.opencode/bin/opencode and is not on PATH: the project mixin
# installs a wrapper at /usr/local/share/npm-global/bin/opencode that clears
# the kit-injected provider variables and execs this binary.
ARG OPENCODE_VERSION=2.3.0

RUN export SHELL=/bin/bash \
 && curl -fsSL https://opencode.ai/v2/install \
      | bash -s -- --version "${OPENCODE_VERSION}" --no-modify-path \
 && /home/agent/.opencode/bin/opencode --version

# Every outbound request must be deliberate; no telemetry.
RUN pio settings set enable_telemetry no

# Pin both platforms so the image is reproducible. The project's
# platformio.ini stays unpinned and uses the installed versions; a toolchain
# bump is a deliberate rebuild.
RUN pio pkg install --global --platform atmelavr@5.3.0 \
 && pio pkg install --global --platform native@1.2.1

WORKDIR /home/agent/workspace

USER agent
