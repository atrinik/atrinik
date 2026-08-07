---
name: atrinik-c-change
description: Implement and validate Atrinik C17 code, headers, native builds, warnings, formatting, tests, and public APIs. Use when native client, server, libatrinik, CMake, or generated-consumer code is the primary change.
---

# Atrinik C Change

Use this skill for native implementation work whose primary contract is C or
C++. Use `atrinik-protocol-change` instead when the wire contract is primary,
and combine this skill with `atrinik-multi-repo-workspace` when more than one
standalone repository is involved.

## Establish ownership

1. Read the workspace and selected component `AGENTS.md` files.
2. Select the owning component through a workspace profile; do not edit a dirty
   primary checkout or copy implementation into the wrapper repository.
3. Trace declarations, implementations, generated inputs, callers, and tests
   before changing an API. `client` and `server` are C17 applications;
   `libatrinik` owns reusable native libraries; `protocol` owns generated
   command identifiers.
4. Preserve component boundaries. Move generally reusable code into
   `libatrinik` only when its ownership and consumer contract are clear.

## Implement the contract

- Follow each repository's `.clang-format` and existing naming, allocation,
  logging, error, and ownership conventions.
- Update the authoritative source rather than generated output. For protocol
  identifiers, edit `protocol/schema/game-commands.json` and regenerate. For
  Flex or export-definition output, update the checked-in source input.
- Update CMake source lists such as `src/cmake.txt` when files are added or
  removed. Keep public headers minimal and document lifetime and ownership at
  the boundary.
- Treat compiler warnings, sanitizer findings, and static-analysis reports as
  defects. Do not suppress a diagnostic without explaining the local contract.
- Add focused unit or integration coverage for success, boundary, failure, and
  cleanup paths. Preserve deterministic behavior where state or ordering is
  observable.

## Validate through the workspace

Build and test every affected native component with the selected profile:

```sh
./atrinik profile show PROFILE
./atrinik build COMPONENT --profile PROFILE --test
```

Build downstream consumers when a public `libatrinik` API or generated
protocol identifier changes. Run repository-specific generation checks,
formatters, sanitizers, static analysis, and the documented `linux-coverage`
preset for substantial native logic changes, then run `git diff --check` in
each changed repository. Keep gcovr source and test exclusions intentional.

If behavior requires a live client/server check, follow the complete
`topology show`/`up`/`ps`/`logs`/`down` lifecycle from the workspace guide and
use `atrinik-test-scenario` when a ready account and character are useful.
