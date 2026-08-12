---
name: atrinik-content-change
description: Change authored content on `main`, including maps, archetypes, media, and scripts.
---

# Atrinik content change

`content@main` is the sole authored source for replacement and Classic targets.
Use a `content` worktree and select the target-specific publisher during
validation; never edit or create a PR against the retired `1.x` line. Retained
`content-1x` checkouts, artifacts, and records are immutable migration or
release evidence. Add `atrinik-multi-repo-workspace` for cross-repository work.

## Establish the contract

1. Select the exact checkout/branch and read its root and nearest `arch/` or
   `maps/` guides.
2. Use `tools/content_catalog` for stable identities and references; trace each
   affected map, archetype, animation, image, artifact, treasure, faction,
   interface, and script.
3. Preserve layout, formatting, case, attribution, and per-asset licenses.
   Exclude unrelated normalization and never mask missing references.
4. Treat embedded Python as Classic runtime code. Isolate generated runtime
   output and never overwrite mutable server state.

## Validate

Run the checkout's aggregate contract, schema, syntax, and runtime validator:

```sh
python3 tools/validate.py
git diff --check
```

Run focused or read-only world audits only when relevant. For live behavior,
select the exact worktree in a coherent profile and use wrapper-native
build/topology commands plus `atrinik-test-scenario` for repeatable state.
