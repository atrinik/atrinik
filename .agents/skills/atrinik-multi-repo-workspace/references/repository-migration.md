# Pre-split classic repository migration

Read this reference only when a workspace still contains the five former
standalone classic repositories.

Initialize only the maintained monorepo destination, then plan, apply, and
audit through the wrapper:

```sh
./atrinik init classic
./atrinik migrate repositories --dry-run
./atrinik migrate repositories --dry-run --json
./atrinik migrate repositories --apply
./atrinik migrate repositories --audit --json
```

Do not run additive `init --with classic` first: its default-cohort preflight
must reject former classic repositories occupying replacement paths.

Migration proves repository identity, history, worktrees, destinations,
profiles, and topology safety before changing anything. It accepts former
canonical or `legacy-*` paths, preserves recoverable originals and linked
worktree directories, and may repair only required Git administrative links.
It refuses ambiguous identities, conflicting occupants, unsafe Git states,
live affected topologies, and selectors it cannot prove. Resolve a reported
condition and rerun the full dry run; never move paths manually to bypass it.

Integrated commit-map targets remain bridge parents. If a verified local
branch-only commit no longer has a published rewritten target, migration
imports that exact local commit; never recreate or depend on a retired
`history/*` namespace.

Classic profile rewrites are atomic and select one complete monorepo root for
all five logical components. The internal `migrated-worktree` selector is
provenance for an exact historical `content-1x` worktree only; `profile set`
must never create it. New classic content work uses a normal `content-1x`
worktree.

Migration does not initialize other repositories or move/reinterpret content,
states, builds, runtimes, scenarios, topologies, or logs. After a successful
apply and audit, `./atrinik init --with classic` may initialize the remaining
classic cohort.
