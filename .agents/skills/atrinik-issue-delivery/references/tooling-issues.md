# Local tooling-issue ledger

Use this optional protocol only when a durable local diagnostic will help future
work. Do not routinely inspect or update either agent ledger, and do not require
status lines in responses. Absence, malformed content, lock contention and
reporting errors never block implementation, validation, PR readiness or handoff.
These diagnostic files are separate from authoritative delivery ownership ledgers.

## Ignored human-readable ledger

Use `build/agent-tooling-issues.md`. The repository's `/build/` ignore rule
keeps it out of `git status`, release inputs, and package manifests. The file
is optional local state: never commit, publish, cite, or copy it into an issue,
PR, delivery report, or other repository evidence. Keep sensitive details out
of both the file and responses. The process-improvement ledger uses the same
facility and the same local-only boundary.

## Supported edit path

never manually edit, overwrite, truncate, or hand-roll either ledger. Use the
wrapper from the active checkout; it resolves a linked worktree to the shared
wrapper root, validates the latest bytes, holds `build/.agent-ledgers.lock`
across merge and publication, and returns the new digest:

```sh
./atrinik agent-ledger update --ledger tooling-issues \
  --key 'mechanism=<slug>;remediation=<slug>' --status open \
  --observation 'generic bounded symptom' --impact 'bounded impact' \
  --recommended-action 'safe next action' [--expected-digest DIGEST]
./atrinik agent-ledger update --ledger process-improvements \
  --key <slug> --status observed --observation 'generic observation' \
  --expected-benefit 'bounded benefit' --related none \
  --observed-at 2026-01-01T00:00:00Z [--expected-digest DIGEST]
```

Pass the prior helper digest for a fail-closed compare-and-swap; use
`--expected-digest absent` when the target must still be absent. Without a
digest, the helper merges the named row into the bytes read under its lock.
Use `--non-blocking` to avoid waiting for optional reporting; on contention,
skip the observation or retry later if useful. On a stale digest or uncertain publication, reread the
latest helper output before retrying. A lock coordinates one shared filesystem;
separate filesystems need an explicit coordinator/event handoff.

Use this exact table, with one row per stable key:

```markdown
# Agent tooling issues

| Stable key | Status | Observation | Impact | Recommended action |
| --- | --- | --- | --- | --- |
| `mechanism=<slug>;remediation=<slug>` | open | generic observation | bounded impact | next action |
```

The key is lowercase ASCII and contains only stable mechanism and remediation
slugs. Do not put timestamps, exception text, usernames, hostnames, paths,
issue numbers, commit IDs, or machine identifiers in it. When the same
mechanism and remediation recur, update that row's status, observation, impact,
and recommended action. Add a row only when either mechanism or remediation is
materially different; duplicate keys are invalid.

Use only the columns above. Never record credentials, tokens, passwords, API
keys, private keys, authorization material, cookies, private host data, or
secret values. Use generic labels and bounded symptoms instead. Tooling
findings may motivate a separately authorized environment-improvement issue,
but do not create or mutate product issues during delivery.

## Validation

Normal guidance checks do not inspect optional local diagnostic state. Explicit
ledger updates still validate bounded UTF-8, schemas, stable keys, secret-like
content and atomic publication under the existing lock. Use the guidance
inventory's `--diagnose-ledgers` mode when troubleshooting these files; diagnostic
findings never change the normal guidance check's exit status.
