# ADR-0004 — ASCII serial protocol

- **Status:** Accepted
- **Date:** 2026-11-07
- **Deciders:** Project owner, firmware architect

## Context

A host (or a human with a serial monitor) must start/stop runs, jog, trigger a
test photo, read status, and change calibration values. The link is the Uno's
USB serial port at 115200 baud. The firmware is small and the host may be a
terminal, a script, or a future GUI.

Options:

1. **Binary protocol** — compact and unambiguous, but hard to debug by hand and
   needs a matching host implementation.
2. **JSON lines** — self-describing, but parsing and printing JSON on an
   ATmega328P costs flash/RAM and is overkill.
3. **ASCII line protocol** — human-readable, easy to script, small parser.

## Decision

Use a **line-based ASCII protocol**:

- 115200 8N1, `\n` terminated, `\r\n` accepted.
- Commands are case-insensitive, at most 63 characters.
- Responses: `OK`, `ERR <CODE> <message>`.
- Configuration: `CFG <key>=<value>`.
- Status: `STATUS <key>=<value> ...`.
- Asynchronous: `EVT <NAME> [key=value ...]` and `LOG <LEVEL> <message>`.
- Long operations return `OK` immediately and report completion with events.
- Protocol version `1`, reported by `VERSION` and `EVT READY`.

## Consequences

Positive:

- A human can debug with any serial monitor.
- Scripts and GUIs parse simple lines.
- No JSON/parser library, no dynamic allocation.
- Events keep the loop non-blocking.
- The protocol is versioned and extensible (new keys/events).

Negative:

- More bytes than a binary protocol; irrelevant at 115200 for these rates.
- No checksum or flow control; USB CDC is reliable enough, and the watchdog
  covers a hung host.
- Human-readable messages can change; machine code should use the `ERR` code and
  `EVT` names, not the free-text message.
- `CONFIG` is a multi-line burst; the host must read until `OK CONFIG`.

## Alternatives rejected

- **Binary:** poor debuggability, extra host tooling.
- **JSON:** flash/RAM and parsing complexity on AVR.
- **Command-line-only with no events:** the host would have to poll, and
  long-running operations would be awkward.
