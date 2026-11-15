# 13 — Sandboxed agent environment

This document describes how the implementing agent is run. It is written for
the human who operates the project. It does not describe firmware behaviour.

## 1. Chosen variant

The agent runs inside a **Docker Sandboxes** (`sbx`) microVM:

- **microVM, not container.** Each sandbox has its own Linux kernel, its own
  Docker daemon and its own network namespace.
- **Clone-mode workspace.** The repository is mounted read-only and the agent
  edits a private clone inside the VM. Work leaves the sandbox through a
  `sandbox-<name>` Git remote that the human fetches and merges.
- **Deny-by-default network.** The agent reaches only the destinations on an
  explicit allowlist. This is the exfiltration control.
- **Cached toolchain and project image.** PlatformIO, the AVR toolchain, the
  native test toolchain, OpenCode 2 and the project's libraries are installed
  once into an image, so a fresh sandbox starts ready.
- **Persistent OpenCode data.** Conversation history, provider auth and
  snapshots live in a block volume that survives sandbox removal.
- **No hardware access.** The agent never touches the board. Flashing and the
  serial monitor stay on the host.

## 2. Why this variant

- The permission model in [PERMISSIONS.md](../PERMISSIONS.md) is prompt-based.
  Its own analysis names the two holes: shell is a universal escape hatch, and
  native tests execute agent-authored code on the host. A VM removes both
  without removing the agent's freedom inside it.
- A shared-kernel container is not enough for untrusted code. The microVM gives
  a separate kernel and hides the host filesystem, SSH keys, credential stores,
  the host Docker socket and every other repository.
- Clone mode protects the host repository from modification, which direct
  mounts do not. Human review of the fetched diff replaces "ask before every
  command".
- The agent only needs to build and test. It does not need the board, so the
  usual reason to avoid a VM — device passthrough — does not apply.

## 3. Trust model

| Protected | Not protected |
|---|---|
| Host files outside the mounted repository and the sandbox data volume | The repository contents, readable through the read-only mount, including untracked and ignored files |
| SSH keys, credential stores, other repositories, browser data | The private clone: the agent may edit, delete and commit there |
| Host Docker daemon and host processes | Destinations on the network allowlist; data can leave to them |
| Outbound traffic to destinations not allowed by policy | The sandbox data volume: the agent owns its own history |
| API key values, which the proxy injects on the host | Host-side local stdio MCP servers, if any are registered |
| Host working tree and `.git`, which are mounted read-only | Clipboard writes, which are allowed; image paste is off by default |

Read-only protects the repository's integrity, not its confidentiality. Do not
keep secrets in the repository.

## 4. Layout

| Location | Contents |
|---|---|
| Host working tree | Repository; mounted read-only inside the sandbox |
| Host `.sandbox/` | Toolchain and project Dockerfiles, the image build script and the state/policy mixin |
| Sandbox VM filesystem | OpenCode 2, the toolchain and project libraries, packages installed during the session, the private clone |
| Sandbox data volume, `/home/agent/.local/share/opencode` | OpenCode database, provider auth, snapshots, tool output |

The volume is keyed on the sandbox name and the path. The sandbox name and the
kit source path must stay stable across resets.

## 5. Setup

### 5.1 Prerequisites

- The `sbx` CLI, version 0.50.0 or later, and a Docker Hub account.
- Git, and a container image builder for the toolchain image.

### 5.2 Sign in and harden the defaults

```bash
sbx login
sbx settings set ssh.agentForwardingEnabled false
sbx settings set skills.defaultMode off
sbx settings set telemetry.disabled true
sbx daemon restart
```

The same settings are applied by `.sandbox/init-host.sh`, which checks the
`sbx` version and the Docker daemon first. `sbx login` stays a human step.

Agent forwarding off means the sandbox cannot ask the host SSH agent to sign or
authenticate. The upstream OpenCode kit still injects provider variables into
the container; the state mixin installs a wrapper that clears them before the
agent process starts, and the sandbox network policy denies GitHub and Copilot.
No GitHub credential reaches the agent, so it cannot push.

### 5.3 Network policy

The sandbox’s allowlist is declared by the state/policy mixin, not by host-wide
CLI rules. It is per sandbox and applied at creation. The host default remains
closed:

```bash
sbx policy init deny-all
```

The mixin allows only:

- `opencode.ai` and `*.opencode.ai` for the OpenCode Go login and models;
- `registry.platformio.org`, `api.registry.platformio.org` and
  `dl.registry.platformio.org` for PlatformIO;
- `pypi.org` and `files.pythonhosted.org` for Python packages.

It denies GitHub, Copilot, the other model providers, npm, Debian and DHI. The
upstream OpenCode kit pre-provisions a broad allowlist; the per-sandbox denies
narrow it. The kit’s `localhost` rule for the MCP gateway is left untouched.

Use `sbx policy ls film-scanner` to review the effective rules, `sbx policy
check network <host>` to test one, and `sbx policy log film-scanner` to see what
the sandbox tried to reach. Do not add `**` globally. For research, use
`sbx policy approval ls`, `sbx policy approval inspect <id>` and
`sbx policy approval respond <id> allow`; the grant applies to matching requests
in this sandbox and the blocked operation can be retried. A temporary
`sbx policy allow network --sandbox film-scanner <host>` is also available.

### 5.4 Toolchain and project images

Two images are built in order. The first is the base; the second is scoped to
this project.

[`.sandbox/toolchain.Dockerfile`](../.sandbox/toolchain.Dockerfile) starts from
the OpenCode sandbox template and installs `build-essential`, Python, `pipx`,
PlatformIO, the `atmelavr` platform, the `native` platform and OpenCode `2.3.0`.
The OpenCode v2 installer is used because the v1 installer refuses 2.x; the
binary lands at `/home/agent/.opencode/bin/opencode` and the template's npm
OpenCode `1.19.5` is removed. The version is the `OPENCODE_VERSION` build
argument. This image contains no project content.

[`.sandbox/tools.Dockerfile`](../.sandbox/tools.Dockerfile) builds on
`film-scanner-toolchain:v1` and adds the project dependencies. It uses the
repository's real `platformio.ini`, sets
`PLATFORMIO_LIBDEPS_DIR=/home/agent/.platformio/libdeps` and installs the
declared `lib_deps`, GoogleTest for the native tests and Unity. The libraries
are present in the VM when it starts; the clone does not download them. `.pio`
build output remains local to the clone.

```bash
./.sandbox/build-image.sh
```

The script builds both images, exports `film-scanner-tools:v1` to a temporary
tar and loads it as the sandbox template; the tar is deleted after the import.
Rebuild when the base toolchain, `OPENCODE_VERSION` or
`platformio.ini` changes, then recreate the sandbox; an existing sandbox keeps
the image it was created from. Use `docker build --no-cache` for the base image
to refresh OpenCode or the toolchain, and for the project image to refresh
floating library dependencies deliberately.

The agent kit is pinned separately in the start script:
`docker.io/sbx/opencode:2.3.0`. The OpenCode application version and the kit
version must be bumped together.

Unity is not referenced by `platformio.ini` today. It is installed so the
image is ready for later embedded tests; PlatformIO will not use it until a
`lib_deps` or test-framework entry names it.

### 5.5 State and policy mixin

[`.sandbox/opencode-state/kit.yaml`](../.sandbox/opencode-state/kit.yaml)
declares three capabilities:

- **`volume@1`** — a 2 GB volume at
  `/home/agent/.local/share/opencode`, mode `0755`.
- **`lifecycle@1`** — chowns the volume to `1000:1000`, then installs
  `/usr/local/share/npm-global/bin/opencode` as a wrapper. The wrapper unsets
  the provider variables the upstream OpenCode kit injects (`GH_TOKEN`,
  `GITHUB_TOKEN`, `OPENAI_API_KEY`, `ANTHROPIC_API_KEY`,
  `OPENROUTER_API_KEY`, `XAI_API_KEY`, `GOOGLE_GENERATIVE_AI_API_KEY`,
  `GROQ_API_KEY`) and execs `/home/agent/.opencode/bin/opencode`. Kits have no
  environment field; this lifecycle command is the kit-level way to keep those
  values out of the agent process.
- **`network-policy@1`** — the per-sandbox allow/deny rules described in
  §5.3, applied to both the `install` and `runtime` phases.

The volume is keyed on the sandbox name and the path. The sandbox name and the
kit source path must stay stable across resets.

### 5.6 Create the sandbox

```bash
./.sandbox/start.sh
```

The script builds the images and loads the template if
`film-scanner-tools:v1` is missing, then runs
`sbx run docker.io/sbx/opencode:2.3.0` with the name, clone mode, template and
kit. The first run caches the template, creates the clone and attaches the data
volume. Log in to OpenCode Go inside the sandbox on the first session; the auth
is stored in the volume and survives reset. A read-only token is sufficient for
OpenCode Go; no write-scoped provider credential is needed.

## 6. Daily use

### 6.1 Reattach

```bash
./.sandbox/start.sh
```

Files and installed packages persist across stop and start. Reattaching is not
a clean session.

### 6.2 Clean reset

Merge anything worth keeping first; removal deletes the private clone. See
section 6.3 for the fetch and review.

```bash
./.sandbox/start.sh --reset
```

This removes the sandbox and creates a fresh one. The new VM is clean, the clone
is fresh, the toolchain and project libraries come from the image and the
conversation and provider login come back from the volume. It reuses the
existing template; rebuild with `.sandbox/build-image.sh` first if the
toolchain or `platformio.ini` changed. Do not use `sbx reset` for this: it is
global and also clears cached images.

### 6.3 Retrieve the work

The clone follows the branch checked out on the host when the sandbox was
created. Fetch it, review it, merge it:

```bash
git fetch sandbox-film-scanner
git diff HEAD...sandbox-film-scanner/<branch>
git merge sandbox-film-scanner/<branch>
```

Nothing reaches the host working tree without this step.

### 6.4 Flash and monitor on the host

```bash
pio run -e uno -t upload
pio device monitor -b 115200
```

The agent's definition of done inside the sandbox is `pio test -e native`,
`pio run -e uno` and `./scripts/check_docs.py`. Hardware acceptance remains a
host activity.

## 7. What persists

| State | Survives a clean reset? |
|---|---|
| Conversation history, provider auth, snapshots | Yes, data volume |
| OpenCode 2, the toolchain and project libraries | Yes, cached image |
| Repository clone and uncommitted edits | No, deleted with the sandbox |
| `.pio` build output | No, inside the clone |
| Packages installed during a session | No, VM filesystem |
| Shell history and temporary files | No, VM filesystem |

The OpenCode Go login is an in-sandbox login stored in the data volume. The
upstream kit’s proxy-managed provider variables are cleared by the state mixin’s
wrapper before the agent starts; GitHub and the other providers are denied at
the network layer.

## 8. Operating rules

- Keep the sandbox name and the `.sandbox/` path stable. Changing either
  detaches the existing volume; the history is retained but not mounted.
- Do not change the volume `size`, `mode` or `tmpfs` setting on an existing
  sandbox. A difference is refused against retained storage. Removing the
  volume deletes the history.
- Do not save a clone-mode sandbox as a template. Template saves capture the VM
  filesystem, including the clone and any credentials left in it.
- Do not register local stdio MCP servers. They run on the host, outside the
  VM.
- Do not add GitHub credentials or re-enable SSH agent forwarding. The state
  mixin clears the kit’s provider variables for the agent and the network policy
  denies GitHub and Copilot; do not re-add either.
- The agent kit is pinned to `docker.io/sbx/opencode:2.3.0` and the project
  image installs OpenCode `2.3.0`. Bump both together and rebuild the template.
- The network allowlist belongs in the state mixin. Do not add host-wide allow
  rules; use per-sandbox approvals for ad-hoc research.
- The image is project-scoped. A change to `platformio.ini` or
  `OPENCODE_VERSION` means rebuilding both images, reloading the template and
  recreating the sandbox; an existing sandbox keeps its original image.
- Do not expect `.pio/libdeps` in the clone. Dependency sources come from the
  image through `PLATFORMIO_LIBDEPS_DIR`; only `.pio/build` is project-local.
- Review the fetched diff before merging. Clone mode is the boundary for the
  repository; the sandbox is the boundary for the machine.

## 9. Agent permissions

[`opencode.jsonc`](../opencode.jsonc) and
[PERMISSIONS.md](../PERMISSIONS.md) continue to apply. The sandbox is an
additional layer, not a replacement.

Relaxing shell prompting inside the sandbox is a separate, deliberate change.
It also needs a decision about commits in the clone first: `AGENTS.md` says
never commit without asking, while clone mode needs the agent to commit so work
can be fetched.

## 10. See also

- [ADR-0011 — Sandboxed agent development](adr/0011-sandboxed-agent-development.md)
- [PERMISSIONS.md](../PERMISSIONS.md) — agent policy
- [09-testing.md](09-testing.md) — native tests and hardware acceptance
- [11-implementation-plan.md](11-implementation-plan.md) — implementation order
