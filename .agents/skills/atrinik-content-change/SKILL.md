---
name: atrinik-content-change
description: Change and validate Atrinik maps, archetypes, graphics, animations, artifacts, treasures, factions, interfaces, quests, and embedded scripts. Use when authored files or content collection in the content repository are primary.
---

# Atrinik Content Change

Use this skill for authored world data in the standalone `content` repository.
Combine it with `atrinik-multi-repo-workspace` when a server, client, resource,
or tooling change is also required.

## Establish the content contract

1. Read the workspace guide plus `content/AGENTS.md` and the nearest nested
   guide under `arch/` or `maps/`.
2. Work in a selected content worktree. Do not write generated runtime output
   back into the source tree or replace a mutable server-data directory.
3. Use `tools/content_catalog` as the authoritative identity and cross-reference
   layer. Domain-qualified stable IDs are contracts; display names, filesystem
   order, and runtime table positions are not identities.
4. Trace every map path, archetype, animation, image, artifact, treasure,
   faction, interface, and script reference touched by the change.

## Make the change

- Preserve established file layout, formatting, naming, attribution, and
  per-asset licensing. Keep unrelated normalizer churn out of the diff.
- Keep map and archetype references portable and case-correct. Never hide a
  missing reference with a local absolute path or a generated placeholder.
- Treat embedded Python as runtime code: preserve engine-owned entry points,
  avoid nondeterministic side effects, and validate failure behavior.
- Use `tools/world_content_audit.py` only for read-only exploratory summaries.
  It does not replace `tools/validate.py` or the catalog, and report findings
  are not generated source.
- The standalone `tools` repository owns the map checker. Do not copy its
  implementation or bypass its released content-catalog dependency.

## Validate

Run the canonical content validator and build an isolated runtime tree:

```sh
python3 tools/validate.py
python3 tools/build_runtime.py --output /tmp/atrinik-content-runtime
python3 tools/world_content_audit.py all
```

Use a task-specific temporary directory rather than the literal example when
concurrent work is possible. Run focused collection or audit commands for the
changed domain, then `git diff --check`.

For live behavior, select the content worktree in a workspace profile, build
the affected server/client components with `./atrinik build ... --test`, and
hand off the exact wrapper-native topology lifecycle. Use
`atrinik-test-scenario` for repeatable gameplay state.
