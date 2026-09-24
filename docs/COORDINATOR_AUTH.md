# Host GitHub authentication

Native Linux development, Git/GitHub, review and delivery coordination use the
actual passwd user's standard private GitHub CLI store. Each delivery retains
its own worktree, leases, private Codex state and isolated mutable caches.
Short-lived pinned CPU build workers receive no credentials or signing agents.
Authentication supplies identity and API capabilities, never authorization for
an issue mutation, merge, release, governance change or other unrequested action.

## Native authentication and capability preflight

Follow [the accepted native execution contract](LINUX_EXECUTION.md) before
issue/project preparation. An unaccepted candidate cannot authorize its own
delivery. Keep `HOME` equal to the actual passwd home. The protected helper uses
that user's standard `~/.config/gh`; it intentionally strips `GH_CONFIG_DIR`.
A working alternate store is not proof that helper authentication works.
Never change the filter, copy tokens between stores, mount keyrings or introduce
an authentication proxy. Keep the directory user-owned/private and retain other
mutable credential stores separately.

Use one selector-free native delivery shell for capability preflight and every
subsequent helper operation. The helper filters `GH_CONFIG_DIR` and
`XDG_CONFIG_HOME` but retains token environment overrides, so clear all four:

```sh
unset GH_CONFIG_DIR XDG_CONFIG_HOME GH_TOKEN GITHUB_TOKEN
```

Reuse the existing standard login. Batch the selected repository, Project and
package capabilities before genesis or resume, without `--show-token` or HTTP
debug logging:

```sh
gh api user --jq .login
gh api repos/atrinik/atrinik --jq '{full_name,permissions}'
gh api graphql -f query='query { organization(login:"atrinik") { projectsV2(first:10) { nodes { id title } } } }'
gh api 'orgs/atrinik/packages/container/linux-build/versions?per_page=1' --jq '.[0].name'
```

Use the actual selected physical repository and paginate when needed. Verify
actor and task capabilities together; available scopes cannot exceed repository,
package or organization/SSO access. Package metadata does not prove the exact
registry manifest, provenance or runtime behavior. The live helper's authenticated
genesis or exact resume remains the final actor/ownership proof.

## Host-owned login maintenance

Only the host owner logs in, refreshes, switches accounts or changes credential
configuration. Reuse a working login. If the standard private store needs a
file-backed refresh, the owner runs one combined action for the missing scopes:

```sh
env -u GH_CONFIG_DIR -u XDG_CONFIG_HOME -u GH_TOKEN -u GITHUB_TOKEN gh auth refresh \
  --hostname github.com --scopes project,workflow,read:packages --insecure-storage
```

If no standard login exists, the host owner uses `gh auth login` with that
hostname/scopes and `--web --insecure-storage`. This is not a worker-initiated
login or token-copy operation. A keyring login may work interactively but fail in
the helper's protected environment; do not work around that by copying secrets.
Never print `hosts.yml`, tokens or raw authentication headers. Never pass a token
through Docker environment options, build arguments, images, Git files, ledgers,
reports or PR bodies. Host Docker registry authentication is separate and is
never mounted into build/runtime workers.

After refresh, expiration recovery or account switching, recheck the actor and
all task capabilities. A changed actor requires the helper's supported ownership
flow; it never silently inherits the old ledger. Coordinate account changes
across active deliveries. A 403 requires the exact missing scope, repository or
SSO capability to be corrected; do not broaden permissions speculatively.

## Native recovery and approval boundaries

Keep the exact host, passwd UID/home, private Codex home, native filesystem/root
identities, issue/PR mode, ledger, registered worktree/branch/head, caches and
mutable runtime state. Build-worker exit does not terminate native ownership.
Reconnect reruns the public probe, complete collision inventory, target proof,
fresh CAS and ordered leases. Names, old probes and timestamps are discovery
evidence only; no implicit transfer to a container or another user is permitted.

Separate authentication (who/capabilities), task permission (what the user
requested), and execution approval (sandbox). An already-authorized issue or
Project action needs no second conversational permission request. Report the
specific failed capability and command. Gather missing scopes into one host
action, keep it pending, and continue independent work. A lack of reply is not
approval; do not repeat browser-login requests on every continuation. Project
coordinators own shared host-setup requests; leaves report missing capabilities.

Batch related read-only execution requests and use an already-approved narrow
rule where applicable. Never disable the sandbox, install blanket GitHub/Docker
allow rules or change managed policy as an authentication workaround. Personal
execution settings are not repository credentials or composition defaults.

## Existing bound container compatibility

Historical bound coordinators retain their existing exact read-only directory
bind from the selected private host store to `/home/ubuntu/.config/gh`. The
helper's protected environment and ordinary `gh` use that store through `HOME`;
`GH_CONFIG_DIR` overrides do not replace proof. Retain the actual mount source,
user mapping, private modes and actor. Bind-directory identity allows existing
host-side atomic updates to remain visible; a saved marker proves no ownership.
Do not recreate/remount a coordinator, switch its account or copy credentials
to adopt this guidance. Keep token environment overrides unset. A keychain-only
store cannot be repaired by exposing the host keyring, home, `.ssh` or Docker
configuration. An unreadable or mismatched bind stops reuse.

Only trusted existing coordinators may consume that bound login. Read-only
prevents credential-file edits, not API writes or use of its permissions; never
expose it to build workers, fork code, runtime services or benchmarks. Workers
never run `gh auth login`, `refresh`, `logout`, `switch`, `setup-git` or `gh config
set` against it. The owner maintains the exact host store and workers reprove
actor/capabilities afterward. No credential copy or authentication proxy is
permitted. Retain the complete [historical context/worktree/ledger/CAS/lease
compatibility gates](LINUX_EXECUTION.md#existing-bound-container-compatibility).

References: [GitHub CLI login](https://cli.github.com/manual/gh_auth_login),
[environment precedence](https://cli.github.com/manual/gh_help_environment),
[refresh](https://cli.github.com/manual/gh_auth_refresh),
[GHCR authentication](https://docs.github.com/en/packages/working-with-a-github-packages-registry/working-with-the-container-registry).
