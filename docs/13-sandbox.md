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
- **Cached toolchain image.** PlatformIO, the AVR toolchain and the native test
  toolchain are installed once into an image, so a fresh sandbox starts ready.
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
| Host `.sandbox/` | Toolchain Dockerfile and the declaration-only state kit |
| Sandbox VM filesystem | Toolchain, packages installed during the session, the private clone |
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

Agent forwarding off means the sandbox cannot ask the host SSH agent to sign or
authenticate. No GitHub credential is stored either, so the sandbox cannot
push.

### 5.3 Network policy

Start closed and add only what a build needs:

```bash
sbx policy init deny-all
sbx policy allow network "*.platformio.org"
sbx policy allow network "pypi.org,files.pythonhosted.org"
# Add the model provider endpoint and anything else `sbx policy log` reports.
```

Use `sbx policy ls` to review the active rules, `sbx policy check network
<host>` to test one, and `sbx policy log` to see what the sandbox tried to
reach. Do not add `**`.

### 5.4 Toolchain image

The Dockerfile under `.sandbox/` starts from the OpenCode sandbox template and
installs `build-essential`, Python, `pipx`, PlatformIO, the `atmelavr` platform
and a warmed native test toolchain.

```bash
docker build -f .sandbox/tools.Dockerfile -t film-scanner-tools:v1 .
docker image save film-scanner-tools:v1 -o /tmp/film-scanner-tools.tar
sbx template load /tmp/film-scanner-tools.tar
```

The image is cached locally. Rebuild it only when the toolchain changes.

### 5.5 Persistent agent data

A declaration-only kit creates the volume and fixes its ownership:

```yaml
# .sandbox/opencode-state/kit.yaml
schemaVersion: "3"
kind: mixin
capabilities:
  - type: com.docker.sandbox/volume@1
    config:
      path: /home/agent/.local/share/opencode
      size: 2g
      mode: "0755"
  - type: com.docker.sandbox/lifecycle@1
    config:
      install:
        - command: chown -R 1000:1000 /home/agent/.local/share/opencode
          user: "0"
```

### 5.6 Create the sandbox

```bash
sbx run opencode \
  --name film-scanner \
  --clone \
  -t film-scanner-tools:v1 \
  --kit ./.sandbox/opencode-state \
  .
```

The first run caches the template, creates the clone and attaches the data
volume.

## 6. Daily use

### 6.1 Reattach

```bash
sbx run --name film-scanner
```

Files and installed packages persist across stop and start. Reattaching is not
a clean session.

### 6.2 Clean reset

Merge anything worth keeping first; removal deletes the private clone. See
section 6.3 for the fetch and review.

```bash
sbx rm film-scanner
sbx run opencode \
  --name film-scanner \
  --clone \
  -t film-scanner-tools:v1 \
  --kit ./.sandbox/opencode-state \
  .
```

The new VM is clean, the clone is fresh, the toolchain comes from the image and
the conversation and provider login come back from the volume. Do not use
`sbx reset` for this: it is global and also clears cached images.

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
| Toolchain and build dependencies | Yes, cached image |
| Repository clone and uncommitted edits | No, deleted with the sandbox |
| `.pio` build output | No, inside the clone |
| Packages installed during a session | No, VM filesystem |
| Shell history and temporary files | No, VM filesystem |

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
- Do not add GitHub credentials or re-enable SSH agent forwarding. With both
  absent, the sandbox cannot push.
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
