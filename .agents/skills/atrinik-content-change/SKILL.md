---
name: atrinik-content-change
description: Change and validate Atrinik authored world data. Use for maps, archetypes, media metadata, quests, interfaces, embedded scripts, or content collection on `main` or `1.x`.
---

# Atrinik content change

Use `content@main` for replacement forward authoring and `content-1x@1.x` for
classic maintenance. Never assume a format or generated artifact belongs on
both lines; cross-line changes use separate worktrees, validation, commits, and
pull requests. Add `atrinik-multi-repo-workspace` when another checkout is
affected.

## Establish the contract

1. Select the exact content checkout/branch, then read its root guide and the
   nearest `arch/` or `maps/` guide.
2. Use `tools/content_catalog` for stable domain identities and references.
   Trace every affected map, archetype, animation, image, artifact, treasure,
   faction, interface, and script reference.
3. Preserve layout, formatting, case, attribution, and per-asset licenses.
   Keep unrelated normalization out of the diff. Never mask a missing
   reference with an absolute path or generated placeholder.
4. Treat embedded Python as classic runtime code. Keep runtime output in an
   isolated generated directory and never overwrite mutable server state.

## Validate

Run the checkout's canonical aggregate validator; it already performs contract,
schema, syntax, and runtime-build validation:

```sh
python3 tools/validate.py
git diff --check
```

Run `python3 tools/world_content_audit.py all` only when its read-only report is
relevant, plus focused commands named by the changed domain. For live behavior,
select the exact content worktree in a coherent profile and use wrapper-native
build/topology commands. Use `atrinik-test-scenario` for repeatable state.
