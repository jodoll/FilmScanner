# 13 — Sandboxed agent environment

The implementing agent runs in a Docker Sandboxes (`sbx`) microVM, not on the
host. The sandbox has its own Linux kernel, a deny-by-default network policy,
and no access to the board, the host filesystem or host credentials. The
repository is mounted read-only; the agent edits a private clone inside the
VM, and its work reaches the host only through a `sandbox-film-scanner` Git
remote that the human fetches, reviews and merges. Rationale and rejected
alternatives: [ADR-0011 — Sandboxed agent
development](adr/0011-sandboxed-agent-development.md).

## Why

The sandbox allows the agent more autonomy while keeping the host safe. The
agent builds, tests and commits inside its clone without a prompt before
every command; the microVM boundary, the deny-by-default network and human
review of the fetched diff protect the host machine and the repository. The
agent only needs to build and test — it never touches the hardware — so no
device passthrough is needed.

## Use

Run from the repository root. One-time setup:

```bash
./sandbox/init-host.sh              # harden sbx settings; check sbx and Docker
sbx login                           # human step
./sandbox/set-opencode-secret.sh    # store the OpenCode API key (see below)
```

Daily use:

```bash
./sandbox/start.sh                  # create or reattach
./sandbox/start.sh --reset          # remove and recreate (fresh clone)
./sandbox/build-image.sh            # rebuild the image
```

`start.sh` builds and loads the sandbox template automatically when
`film-scanner-tools:v1` is missing. Run `build-image.sh` when the toolchain or
`platformio.ini` changes, then `--reset` so an existing sandbox picks up the
new image. Resetting deletes the private clone — merge anything worth keeping
first.

The image ships every dependency the project needs to build: PlatformIO with
the AVR and native platforms, the libraries declared in `platformio.ini` and
GoogleTest. A fresh clone builds and tests without downloading anything.
Rebuild the image when the dependencies change; an existing sandbox keeps the
image it was created from until it is reset.

Retrieving work:

```bash
git fetch sandbox-film-scanner
git diff HEAD...sandbox-film-scanner/<branch>
git merge sandbox-film-scanner/<branch>
```

Nothing reaches the host working tree without this step. Flashing and the
serial monitor stay on the host (`pio run -e uno -t upload`,
`pio device monitor -b 115200`). The agent's definition of done inside the
sandbox is `pio test -e native`, `pio run -e uno` and
`./scripts/check_docs.py`.

## Secret injection

`set-opencode-secret.sh` stores the OpenCode API key as a global `sbx` custom
secret, keyed to the `opencode.ai` and `*.opencode.ai` hosts and the
`OPENCODE_API_KEY` environment variable. The sandbox receives only a
placeholder; the host-side proxy substitutes the real key in request headers
on the way out, so the key value never enters the VM.

The secret is global by design — every sandbox gets it — and is picked up when
a sandbox is created: set it before the first `start.sh`, and `--reset` after
setting or updating it. No GitHub credential exists in the sandbox and the
network policy denies GitHub, so the agent cannot push.

## What persists

| State | Survives a clean reset? |
|---|---|
| Conversation history, OpenCode Go auth, snapshots | Yes, data volume |
| OpenCode, toolchain, project libraries | Yes, cached image |
| Repository clone and uncommitted edits | No, deleted with the sandbox |
| `.pio` build output, packages installed in a session | No, VM filesystem |

## See also

- [ADR-0011 — Sandboxed agent development](adr/0011-sandboxed-agent-development.md)
- [09-testing.md](09-testing.md) — native tests and hardware acceptance
