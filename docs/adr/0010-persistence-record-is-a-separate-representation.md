# ADR-0010 — The persistence record is a separate representation

- **Status:** Accepted
- **Date:** 2026-11-08
- **Amends:** [ADR-0003](0003-config-persistence.md) (the record is not packed and
  does not reuse the domain types)
- **Deciders:** Project owner, firmware architect

## Context

The persisted struct used to be assembled from the domain config slices
(`NotchConfig`, `ButtonConfig`, `MotionConfig`, …) and declared `packed` so that
its size and offsets were fixed. That coupled two things that should not be
coupled:

- **The storage format depended on the domain types.** Adding, removing or
  reordering a field in a slice silently changed what was on the EEPROM. The
  domain type could not evolve without breaking stored data.
- **The domain types had to be packed.** `__attribute__((packed))` sets
  `alignof` to 1, so `alignof(MotionConfig)` became 1 and every
  `const MotionConfig&` and every slice pointer could be unaligned. On the host
  build that is a `-Waddress-of-packed-member` warning and byte-wise access; on
  the AVR it is harmless but still a smell, and it spreads: `Config` itself
  becomes partially unaligned.

The trigger was a 2-byte size difference between the target and the host build.
It was worth asking why a storage detail was allowed to constrain a domain type
at all.

## Decision

1. **`ConfigRecord` is its own type, in its own header.** It is a flat struct of
   plain scalars. It does not contain the domain slices.
2. **No `packed` anywhere.** The record has no nested types, so there is nothing
   to align around. `MotionConfig` and the other slices keep their natural
   alignment and are passed by reference without caveats.
3. **The store deals in bytes.** `INonVolatileStore` offers
   `read(uint8_t*, uint16_t)` and `write(const uint8_t*, uint16_t)`, with no
   address parameter: the record lives at address 0. `EepromStore` implements it
   and knows nothing about `Config` or `ConfigRecord`.
4. **The mapping lives in the core.** `makeRecord`, `applyRecord` and
   `recordHasValidHeader` are free functions beside the record, so the mapping,
   the header rules and the checksum are natively testable. `ConfigManager`
   orchestrates: read bytes, validate, map, validate values, push slices.
5. **The record is self-describing.** Magic value, format version, own length
   and a CRC16-CCITT over every byte before the checksum. A record that fails any
   check is discarded wholesale.
6. **The docs specify behaviour, not layout.** The protocol document states that
   a bad or foreign record is rejected and defaults are used, that `SAVE` is
   explicit and verified by read-back, and that version 1 has no migration. The
   byte layout, the field order and the total size are the record's business.

## Consequences

Positive:

- A slice can gain a field, be reordered or be re-typed without touching the
  stored format. Changing the format becomes a deliberate act.
- No packed attribute, no `alignof == 1`, no unaligned references, no compiler
  warning surface.
- `Config` no longer knows that it is persisted, which is the correct direction
  of dependency.
- The record's rules are testable without EEPROM and without the manager.
- `EepromStore` shrinks to a byte mover, which is what a HAL adapter should be.

Negative:

- The field list exists twice: once as slices, once as record fields. That
  duplication is the point — one is behaviour, the other is a format — but it
  must be kept in step by the mapping code and its tests.
- The record's size and offsets are not identical between the AVR build and the
  host test build. This is accepted: only the target ever writes EEPROM, and the
  host exercises the logic through a fake store. If an offline image tool is ever
  wanted, the record becomes a documented external format and its layout is
  frozen at that point.
- `ConfigManager` owns a little more orchestration.

## Alternatives rejected

- **Pack the domain slices and reuse them in the record (the previous design).**
  Fixed size, but it makes the storage format constrain the domain types and
  leaks `alignof == 1` into every reference. That was the actual defect.
- **Pack only the record, keep nested slices.** Packing a container does not
  shrink a member, so the size still depends on the host ABI; and the record
  still changes whenever a slice changes.
- **Keep the layout in the docs.** It is not observable from the host, and it
  would re-create the coupling through the back door.
- **A `uint8_t` image with hand-written offsets.** Portable and exact, but it is
  more code than a flat struct plus a mapping, and offsets drift silently from
  the field list.
- **No checksum.** A partially written or foreign record would be accepted.
