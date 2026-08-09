---
name: atrinik-c-change
description: Change classic C17/CMake code, headers, and generated consumers; excludes Rust/Go replacements.
---

# Atrinik classic C change

Use this wrapper skill to coordinate native work in the `atrinik/classic`
monorepo. Load `atrinik-protocol-change` when bytes on the wire are primary and
`atrinik-multi-repo-workspace` when another physical checkout is affected.

## Establish ownership

1. Create/select one full `classic` worktree and a classic-derived profile.
   Never edit a dirty primary or treat a module subdirectory as an independent
   repository.
2. Read `classic/AGENTS.md`, the nearest module guide, and the applicable local
   `classic-native-change` or `classic-protocol-change` skill.
3. Trace declarations, implementations, generated inputs, callers, and tests.
   `classic/client`, `classic/server`, and `classic/libatrinik` own native code;
   `classic/protocol/schema/game-commands.json` owns generated command IDs.

Follow the monorepo's formatting, allocation, lifetime, error, and CMake
conventions. Edit authoritative inputs instead of generated output, update
source lists, and cover success, boundaries, failure, and cleanup. Treat
warnings, sanitizer findings, and static-analysis reports as defects.

## Validate

```sh
./atrinik profile show PROFILE
./atrinik build COMPONENT --profile PROFILE --test
```

Build every affected consumer of a public library or generated protocol
change. Run the module's generation/dependency checks, the documented coverage
preset for substantial logic, and `git diff --check`. For live behavior, use
`atrinik-server-runtime` and the complete supervised lifecycle; add
`atrinik-test-scenario` when login state is useful.
