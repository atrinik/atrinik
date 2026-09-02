# Local tooling-issue ledger

Use this protocol whenever repository work encounters a host, tool, transport,
or environment problem. It records environment improvements, not product
defects or issue evidence.

## Response checklist

Every response that performs repository work includes one explicit status line:

```text
Tooling issues: none
```

When a problem was observed, replace `none` with its stable key(s) and a short
current status. Update the local ledger before the response. Do not omit the
line because a failure was transient or worked around.

## Ignored human-readable ledger

Use `build/agent-tooling-issues.md`. The repository's `/build/` ignore rule
keeps it out of `git status`, release inputs, and package manifests. The file
is optional local state: never commit, publish, cite, or copy it into an issue,
PR, delivery report, or other repository evidence. Keep sensitive details out
of both the file and responses.

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

Run the existing guidance check when the ledger contract or file is relevant:

```sh
python3 -m atrinik_workspace.guidance_inventory --check
```

The check proves the path is ignored and untracked. If the optional file is
absent, it passes; if present, it verifies UTF-8, bounded size, the exact table
columns, allowed statuses, stable-key uniqueness, and rejection of secret-like
fields/values and private host paths. It does not require local ledger bytes in a
clean checkout or CI.
