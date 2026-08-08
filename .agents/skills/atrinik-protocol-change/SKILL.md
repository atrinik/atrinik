---
name: atrinik-protocol-change
description: Change Atrinik wire contracts and all producers/consumers. Use for GP1 Protobuf/QUIC, classic packets and IDs, ADS, generated bindings, or compatibility transitions.
---

# Atrinik protocol change

Use this skill when bytes crossing a process or repository boundary are the
primary contract. Add `atrinik-multi-repo-workspace` and the relevant
implementation skill for coordinated consumers.

## Select the authoritative contract

- Replacement Game Protocol 1 lives in the physical `protocol` checkout and
  owns Protobuf/Buf policy, normative QUIC/framing specifications, generated
  Go/Rust contracts, and conformance fixtures.
- Classic numeric command IDs live at
  `classic/protocol/schema/game-commands.json`; packet payloads may be owned by
  classic client/server producers and consumers. Use a classic-derived profile
  and the monorepo's `classic-protocol-change` skill.
- ADS/authored schemas live with their exact `content@main` or
  `content-1x@1.x` format. Metaserver contracts identify their own owner.

Resolve paths rather than assuming the wrapper CWD:

```sh
./atrinik path protocol --profile default
./atrinik path classic-protocol --profile classic
```

Trace schemas, specifications, generated outputs, producers, consumers,
fixtures, tests, tooling, and release dependencies. Define framing, field
order, widths, signedness, byte order, bounds, state transitions, failures, and
compatibility before implementation.

Edit authoritative definitions and regenerate; never hand-edit generated IDs.
Keep parsing bounded and transactional, test malformed/truncated/oversized and
state-order failures, and remove an old path only after every selected consumer
moves. Avoid parallel compatibility paths without an issue-owned removal gate.

## Validate

Run each owning repository's aggregate generation/test contract and every
affected producer/consumer build. Use wrapper builds only for components whose
wrapper adapters exist; do not route replacement GP1 through classic code.
Finish with `git diff --check` in every repository and report the source,
consumers, compatibility decision, and tests. Use a supervised topology and
scenario when runtime proof is required.
