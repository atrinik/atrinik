---
name: atrinik-protocol-change
description: Trace, redesign, implement, document, and validate Atrinik wire contracts. Use when protocol schemas, generated IDs, classic packets, QUIC payloads, ADS, or producer-consumer compatibility are primary.
---

# Atrinik Protocol Change

Use this skill whenever bytes crossing a process or repository boundary are the
primary contract. Combine it with `atrinik-multi-repo-workspace` and the
relevant implementation skill for coordinated consumer changes.

## Trace the whole contract

1. Read every affected repository guide and select all sources with a workspace
   profile before editing.
2. Identify the authoritative contract. Classic command IDs come from
   `protocol/schema/game-commands.json`; payload layouts may still be owned by
   client/server implementations; ADS schemas live with their content format;
   metaserver bootstrap contracts are owned by `metaserver-worker`.
3. Find producers, consumers, generated outputs, fixtures, tests,
   documentation, packet tooling, and release dependencies. Do not assume the
   repository containing a numeric constant owns the protocol.
4. Write down framing, field order, widths, signedness, byte order, lengths,
   nullability, limits, state transitions, error behavior, and compatibility
   policy before implementation.

## Implement one coherent transition

- Edit schemas or other source definitions, regenerate checked-in artifacts,
  and verify generation is clean. Never hand-edit generated IDs.
- Update every in-scope producer and consumer together when compatibility is
  intentionally broken. Delete superseded commands only after all selected
  consumers have moved.
- Reject malformed, truncated, oversized, out-of-order, or impossible input at
  the boundary. Keep parsing transactional so partial messages cannot leak
  half-applied state.
- Avoid parallel old/new paths unless an explicit compatibility window owns
  their removal. Use precise protocol names rather than vague age labels.
- Add golden or round-trip fixtures and negative boundary tests where useful.

## Validate every consumer

Run protocol generation checks, the protocol unit and CMake suites, and wrapper
build/tests for every affected component:

```sh
./atrinik profile show PROFILE
./atrinik build protocol --profile PROFILE --test
./atrinik build COMPONENT --profile PROFILE --test
```

Run `git diff --check` in each repository. For a live transition, hand off the
complete wrapper topology lifecycle and exact observable result; use
`atrinik-test-scenario` when login state is needed. The final report must name
the authoritative source, changed consumers, compatibility decision, and tests.
