# ADR-0011 — Sandboxed agent development

- **Status:** Accepted
- **Date:** 2026-11-12
- **Deciders:** Project owner

## Context

The agent is constrained by prompts in `opencode.jsonc`. That model has two
admitted holes: a shell command can do anything the user can do, and
`pio test -e native` compiles and runs agent-authored code on the host. The
goal is the opposite trade: give the agent more autonomy by moving it into an
environment where autonomy is safe.

The firmware loop does not need the board. The agent edits, builds and runs
native tests; flashing and the serial monitor are human activities on the
host. That removes device passthrough, the usual blocker for virtualised
environments.

The candidate environments were:

- a Docker Sandboxes (`sbx`) microVM;
- a first-party per-container VM tool;
- a Lima VM;
- a Seatbelt (`sandbox-exec`) wrapper, possibly via
  `@anthropic-ai/sandbox-runtime`;
- a Docker devcontainer;
- a full VM;
- a cloud development host.

Relevant properties: a real kernel boundary, control over outbound traffic,
protection of the host repository, a workable daily loop, and no dependency on
hardware access.

## Decision

1. **The agent runs in an `sbx` microVM, not on the host.** Each sandbox has
   its own Linux kernel, Docker daemon and network namespace. The host
   filesystem, SSH keys, credential stores, the host Docker socket and other
   repositories are outside the sandbox.
2. **The workspace is clone mode.** The host repository is mounted read-only
   at `/run/sandbox/source`; the agent works in a private clone inside the VM.
   `sbx` registers a `sandbox-<name>` remote on the host. The human fetches,
   reviews and merges; the sandbox cannot write the host working tree or
   `.git`.
3. **Network egress is deny-by-default.** The policy starts closed and allows
   only PlatformIO, PyPI and the model provider endpoint, extended from
   `sbx policy log` when a build needs more. No `**`. No GitHub credentials and
   no SSH agent forwarding, so the sandbox cannot push.
4. **The build environment is a cached image; the agent's state is a
   persistent volume.** A Dockerfile under `.sandbox/` installs PlatformIO, the
   AVR toolchain and the native test toolchain into a template. A
   declaration-only kit declares a block volume at
   `/home/agent/.local/share/opencode`, which holds the OpenCode database,
   provider auth and snapshots. A fresh sandbox therefore has a clean VM and a
   clean clone without reinstalling the toolchain or losing the conversation.
5. **Reset is remove-and-recreate, not `sbx reset`.** `sbx rm` deletes the VM
   and clone and retains the block volume; recreating with the same sandbox
   name, kit path and volume path reattaches it. `sbx reset` is global and also
   clears cached images, so it is not the per-session reset.
6. **Hardware stays on the host.** There is no device passthrough and no
   network serial bridge. The agent's definition of done is
   `pio test -e native`, `pio run -e uno` and `./scripts/check_docs.py`.
   Flashing and the serial monitor are human commands on the host.
7. **The sandbox is additive to the permission policy.** `opencode.jsonc`
   continues to apply. Relaxing shell prompts inside the sandbox is not part of
   this decision; it is a separate change that also needs a rule for commits
   inside the clone.

The kit and template paths, the sandbox name and the volume path are stable
identities. Renaming the sandbox or moving the kit detaches the retained
volume.

## Consequences

Positive:

- The agent can be given broad autonomy inside the VM without exposing the
  host user account, SSH keys, browser data, other repositories or the host
  Docker daemon.
- Clone mode makes the repository review the control that matters, instead of
  clicking through shell prompts.
- Deny-by-default egress bounds exfiltration and makes every destination a
  deliberate decision.
- A clean VM per session, with the toolchain preinstalled and the conversation
  retained, is what makes the workflow practical rather than a rebuild ritual.
- The host toolchain no longer has to match the toolchain the agent uses.

Negative:

- The sandbox cannot flash or read the serial port; hardware work is bounded
  by the human's availability at the machine.
- Work leaves through a fetch-and-merge step, which adds friction compared
  with a direct mount.
- The read-only mount still exposes every file in the repository, including
  untracked and ignored files. Secrets must not live there.
- The sandbox does not protect the repository itself; clone mode protects the
  host copy, and the human merge protects the branch.
- Toolchain image, kit and volume have identities that must stay stable, and a
  toolchain change means rebuilding the image.
- The current `AGENTS.md` commit rule conflicts with using the clone as the
  transfer channel. This is an open follow-up, not a resolved decision.

## Alternatives rejected

- **A first-party per-container VM.** Per-container hardware isolation, but no
  agent integration, network policy or credential proxy. It would need a
  custom harness around the agent.
- **Lima.** A real VM and simple to start, but egress is open NAT by default,
  there is no per-domain policy or credential injection, and host mounts use
  virtiofs, which is a poor fit for SQLite. It remains the fallback if `sbx`
  proves impractical.
- **Seatbelt / `sandbox-runtime`.** No separate kernel or PID namespace,
  domain filtering needs an external proxy, `sandbox-exec` is deprecated, and
  wrapping OpenCode's shared background server is awkward. It confines files,
  not the system.
- **Devcontainer or Docker Desktop containers.** A shared VM kernel, easy
  accidental exposure of the Docker socket, and no per-agent network policy.
  Coarser for the same operational cost.
- **Full VM.** Strongest isolation, but heavy, manual and unnecessary for a
  build-and-test workload.
- **Cloud development host.** Good firewall isolation, but adds latency, cost,
  and a second copy of the repository and its history.
- **Device passthrough or a network serial bridge.** Unnecessary once flashing
  is accepted as a host activity; both add moving parts and weaken the
  boundary.

## Follow-ups

1. Decide and document the commit rule for work inside the clone.
2. Decide whether to relax `opencode.jsonc` inside the sandbox, keeping the
   repository review and the human merge.
3. Re-evaluate the per-container VM option when the host supports it.
