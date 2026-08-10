---
name: atrinik-content-change
description: Change authored content on `main` or `1.x`, including maps, archetypes, media, and scripts.
---

# Atrinik content change

For issue fixes, assess both `content@main` and `content-1x@1.x`. Shared
authored changes normally need separate worktrees, validation, commits, and
linked PRs on both lines; record an evidence-backed format, consumer, runtime,
or provenance reason for any single-line exception. Never merge lines wholesale
or share generated output. Use `main` for replacement forward authoring and
`1.x` for Classic maintenance; add `atrinik-multi-repo-workspace` for cross-line
work.

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
