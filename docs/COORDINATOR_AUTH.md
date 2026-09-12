# Shared host GitHub authentication

Trusted Atrinik coordinators use one host-owned GitHub CLI login through a
read-only directory bind. Each delivery keeps its own container, worktrees,
leases, Codex home and mutable caches; it does not need another browser login.
The target is ubuntu's standard `~/.config/gh`, so the delivery helper's
protected environment (which strips `GH_CONFIG_DIR` overrides) and ordinary
`gh` both use this same login through `HOME`. Do not change the target to an
arbitrary config path or weaken the helper's environment filtering.
Authentication supplies identity and API capabilities, never authorization for
an issue mutation, merge, release, governance change or other unrequested action.


## Direct native Linux authentication and recovery

This section applies only after the native execution contract is accepted.
An unaccepted candidate cannot authorize its own native delivery. Container
coordinators retain the existing read-only host mount and never switch accounts.

A proven native coordinator uses the actual passwd user's standard
`~/.config/gh` through `HOME`. The helper intentionally strips
`GH_CONFIG_DIR`; a working override such as `gh-atrinik` is therefore not
proof that native helper authentication works. Do not change the filter, copy
tokens between stores, mount keyrings or introduce an authentication proxy.

The host owner configures that standard private store directly. Reuse an
existing working login; if file-backed refresh is needed, the host owner runs:

```sh
env -u GH_CONFIG_DIR -u XDG_CONFIG_HOME -u GH_TOKEN -u GITHUB_TOKEN gh auth refresh \
  --hostname github.com --scopes project,workflow,read:packages --insecure-storage
```

If no standard login exists, the host owner instead uses `gh auth login`
with the same hostname/scopes and `--web --insecure-storage`. This is one
combined host action, never a worker-initiated login or a token-copy operation.
Keep the directory user-owned/private. A keyring login may work interactively
yet fail in the helper's protected environment; authenticated helper genesis
or exact resume is the required final test.

Use one selector-free native delivery shell for capability preflight AND
every subsequent ledger/helper genesis or resume operation. The helper filters
`GH_CONFIG_DIR` and `XDG_CONFIG_HOME` but retains token environment overrides;
clearing selectors only on individual preflight commands is insufficient.

```sh
unset GH_CONFIG_DIR XDG_CONFIG_HOME GH_TOKEN GITHUB_TOKEN
```

Keep those selectors unset for the entire native delivery, then batch capability
checks before genesis or resume:

```sh
env -u GH_CONFIG_DIR -u XDG_CONFIG_HOME -u GH_TOKEN -u GITHUB_TOKEN gh api user --jq .login
env -u GH_CONFIG_DIR -u XDG_CONFIG_HOME -u GH_TOKEN -u GITHUB_TOKEN gh api repos/atrinik/atrinik --jq '{full_name,permissions}'
env -u GH_CONFIG_DIR -u XDG_CONFIG_HOME -u GH_TOKEN -u GITHUB_TOKEN gh api graphql -f query='query { organization(login:"atrinik") { projectsV2(first:10) { nodes { id title } } } }'
env -u GH_CONFIG_DIR -u XDG_CONFIG_HOME -u GH_TOKEN -u GITHUB_TOKEN gh api 'orgs/atrinik/packages/container/linux-build/versions?per_page=1' --jq '.[0].name'
```

Repeat for each affected physical repository. Keep other mutable credentials
private; permissions grant no task authority. Do not report a scope/authentication
error as missing permission for work already authorized.

Native recovery preserves the exact host, actual passwd UID/home, private
Codex home, native filesystem/root identities, issue/PR mode, ledger identity,
dedicated registered worktree/branch/head, caches and mutable runtime state.
Reconnect reruns the public context probe, complete ledger inventory, exact
worktree/target proof, fresh CAS and ordered leases. Names, old probes and prior
timestamps remain discovery evidence only. Never transfer a native record to a
container or another user implicitly; container recovery retains its own exact
image/mount coordinates and existing no-remount rule.

## One-time host setup

The ordinary devcontainer uses `$HOME/.config/gh-atrinik`. If the host already
has a working `gh` login, initialize that private directory once from it. This
transfers the existing credential only between host stores through stdin; no
new browser login is needed, and no token is printed:

```bash
(
  set -o pipefail
  umask 077
  install -d -m 700 "$HOME/.config/gh-atrinik"
  if [ ! -e "$HOME/.config/gh-atrinik/hosts.yml" ]; then
    gh auth token --hostname github.com | \
      GH_CONFIG_DIR="$HOME/.config/gh-atrinik" gh auth login \
        --hostname github.com --git-protocol https --with-token --insecure-storage
  fi
)
GH_CONFIG_DIR="$HOME/.config/gh-atrinik" gh api user --jq .login
```

The explicit file-storage option places the credential in this private host
directory; the original keyring-backed config is not modified. Do not repeatedly
export it or overwrite an established shared login. Use this same `GH_CONFIG_DIR`
prefix for subsequent host checks and refreshes. If there is no working host
login, the host owner authenticates directly into the shared directory once:

```sh
GH_CONFIG_DIR="$HOME/.config/gh-atrinik" gh auth login \
  --hostname github.com --git-protocol https --web --insecure-storage \
  --scopes read:packages,project,workflow
```

Check all task capabilities before worker bootstrap. If the existing login
lacks them, request one combined host refresh for the missing scopes, for example:

```sh
GH_CONFIG_DIR="$HOME/.config/gh-atrinik" gh auth refresh \
  --hostname github.com --scopes read:packages,project,workflow
```

GitHub CLI's usual repository/organization-read scopes are requested by login;
`read:packages` enables private image reads, `project` supports authorized Project
claims, and `workflow` supports deliveries that update workflow files. Verify the
selected actor and actual capabilities; scopes cannot exceed the account's
repository/package access or an organization's approval/SSO policy. Add further
permissions only for the selected task, not speculative organization administration.

An existing file-backed host `gh` directory can also be used with the native
Docker mount below. A config that only references a host OS keychain is
insufficient: the container cannot access that keychain. Do not mount the host's
keyring service, whole home, `.ssh`, or Docker configuration to work around it.
Never print `hosts.yml`, tokens or raw authentication headers in tool output.
Do not pass a token through a Docker environment option, build argument, image
layer, ledger, Git file or PR body. Workers consume the mounted login directly.

GitHub fine-grained PATs can target one organization and selected repositories,
but currently lack GitHub Packages support. A classic PAT/OAuth login can cover
more of this workflow but is not inherently restricted to one organization.
Mounting it read-only does not limit its API permissions: every trusted worker
can exercise those permissions. Do not expose this mount to untrusted build
containers, fork code, runtime services or benchmark containers. Keep privileged
administrative credentials separate from routine delivery authentication.

## Coordinator bootstrap and reuse

The ordinary `.devcontainer/devcontainer.json` declares:

```text
source=${localEnv:HOME}/.config/gh-atrinik,target=/home/ubuntu/.config/gh,type=bind,readonly
GH_CONFIG_DIR=/home/ubuntu/.config/gh
```

For a newly owned native Docker coordinator, add these options to its supported
pinned-image bootstrap, preserving all existing source, user, private Codex-home,
build-cache and lifetime requirements:

```sh
ATRINIK_HOST_GH_CONFIG="$HOME/.config/gh-atrinik"
test -d "$ATRINIK_HOST_GH_CONFIG"
test -r "$ATRINIK_HOST_GH_CONFIG/hosts.yml"
# Options for docker run, not a standalone container launcher:
# --mount "type=bind,source=$ATRINIK_HOST_GH_CONFIG,target=/home/ubuntu/.config/gh,readonly"
# --env GH_CONFIG_DIR=/home/ubuntu/.config/gh
```

Bind the directory, not only `hosts.yml`, so host-side atomic credential updates
are visible. Validate that it belongs to the intended host user, its credential
files are private/readable by the mapped ubuntu UID, and no other host user can
modify the directory. Do not recursively change host permissions. Inspect the
exact container's mount metadata: `/home/ubuntu/.config/gh` must be a read-only bind from
the selected directory. Keep `GH_TOKEN` and `GITHUB_TOKEN` unset in workers:
these environment variables override the mounted login. Confirm effective
identity rather than assuming mount presence proves authentication.

Do not replace an existing coordinator just to adopt this setup.
An already canonical session continues in place; its owner can adopt the mount
at the next supported bootstrap/recovery, with fresh image, configured-mount-path, worktree, and
ledger checks. Independent coordinators may share this read-only directory;
other mutable authentication state remains private. Windows cross-build and
runtime containers do not inherit the mount from the ordinary coordinator.

## Preflight before delivery or expensive work

Run inside the exact coordinator, without `--show-token` or HTTP debug logging:

```sh
gh auth status --hostname github.com
gh api user --jq .login
gh api repos/atrinik/atrinik --jq '{full_name,permissions}'
```

Use the actual selected repository for its permissions check. Issue/PR delivery
still requires the live helper's actor, ownership, Project and ledger preflight;
these examples do not replace it. For package-consuming work, also test:

```sh
gh api 'orgs/atrinik/packages/container/linux-build/versions?per_page=1' \
  --jq '.[0].name'
```

Verify the exact registry manifest as part of artifact acceptance; package
metadata alone is not proof of manifest/provenance or runtime behavior. Where a
registry client needs a bearer token, obtain it from the mounted login within
the coordinator process and keep it in memory; never log or persist a token
copy. Do not introduce an authentication proxy. Credential mount access does
not change any host/container execution or Git sandbox restriction in the task.

## Avoid repeated approval and login prompts

Separate three gates: GitHub authentication (who/capabilities), delivery
permission (what the user authorized), and Codex execution approval (sandbox).
A missing scope needs one host refresh; an already-authorized issue/Project
claim needs no second conversational permission request. Sandbox approval is
separate and still follows the active tool policy. Report which gate failed
with the exact capability and command, not a generic request to authorize GitHub.

At startup, batch the selected task's read-only repository, Project and package
capability checks before implementation or expensive work. Gather all missing
scopes into one host action. Reuse existing valid authentication and permissions;
do not request a fresh login per worker or rediscover missing scopes one at a
time. After requesting a host action, keep it pending, continue independent
work, and recheck after the user reports completion. Do not reissue the same
browser-login request on every continuation; a lack of reply is not approval.
For an ordered program, its coordinator owns the single host-setup request; leaf
workers report missing capabilities to that owner instead of starting competing
OAuth flows. Independent workers consume the prepared host directory and report
a missing prerequisite without launching their own login flow.

Keep the same owned coordinator through the delivery and human authentication
wait. Thirty minutes is the idle deadline, not an unconditional process timer
that kills active work; the normal maximum lifetime is twelve hours. Record
activity/deadlines, honor leases, and stop only owned idle resources. If it stops,
inspect that exact handle and recover once under the existing contract instead
of spawning another empty-auth coordinator.

For repeated Codex execution approvals, batch related read-only calls and use
an already-approved narrow command rule where applicable. Keep read and write
operations separate so the approval's scope is clear. Agents may propose a
concrete narrowly scoped rule for the host owner to review; they must not disable
the sandbox, install blanket `gh`/`docker` allow rules, or change managed policy
as an authentication workaround. Personal execution settings are not repository
credentials and do not belong in this PR's composition defaults. See the
[Codex rules](https://developers.openai.com/codex/rules) and
[security documentation](https://developers.openai.com/codex/security).

## Host-only maintenance and troubleshooting

Workers never run `gh auth login`, `refresh`, `logout`, `switch`, `setup-git` or
`gh config set` against the shared directory. The host owner performs needed
changes. For example, add a missing package-read scope on the host:

```sh
GH_CONFIG_DIR="$HOME/.config/gh-atrinik" gh auth refresh \
  --hostname github.com --scopes read:packages
```

After refresh, expiration recovery or account switching, each active worker
rechecks the actor and task capabilities before proceeding. A different actor
requires the delivery helper's supported ownership flow; it never silently
inherits the old ledger. Coordinate host account switching across live workers.
If the token is revoked, run the host login command again and repeat preflight.

- Missing directory or unreadable `hosts.yml`: finish host setup and check the
  exact mount source/UID; do not fall back to a writable mount or worker login.
- Host works but worker returns 401: check keychain-only storage, expired token,
  wrong mounted directory, and environment overrides without exposing values.
- API returns 403: check the exact missing scope, account permissions and SSO;
  ask the host owner for that capability and rerun the failing read.
- A refresh/config command returns read-only filesystem: run it on the host.

References: [GitHub CLI login](https://cli.github.com/manual/gh_auth_login),
[environment precedence and GH_CONFIG_DIR](https://cli.github.com/manual/gh_help_environment),
[refresh](https://cli.github.com/manual/gh_auth_refresh),
[PAT capabilities and limitations](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/managing-your-personal-access-tokens),
[GHCR authentication](https://docs.github.com/en/packages/working-with-a-github-packages-registry/working-with-the-container-registry).
