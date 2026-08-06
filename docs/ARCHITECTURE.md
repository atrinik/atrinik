# Workspace architecture

## Ownership boundary

This repository owns only orchestration code and the component manifest.
Implementation, release packaging, and component-specific tests belong to the
standalone repositories listed in `components.json`. The workspace coordinator
does not vendor, pin, or commit component source.

`components.json` is the source of truth for repository identity, default
branch, and local build contract. Profile and state schemas are intentionally
strict: duplicate keys, missing fields, unknown fields, invalid names, and
repository mismatches fail before an operation changes data.

Read-only inspection commands keep structured data on stdout and suppress Git
traces so callers can safely consume `status --json`, `worktree list --json`,
`profile show --json`, `state list --json`, and `path`. Status never fetches;
its ahead/behind fields describe the cached canonical remote ref and are null
when that ref is unavailable.

## Managed layout

~~~text
<wrapper>/
  <component>/                       primary independent Git checkouts
workspace/
  worktrees/<component>/<label>/     component Git worktrees
  profiles/<name>.json               checkout selectors
  build/profiles/<name>-<key>/       isolated sources, builds, and runtime
  build/npm-cache/                   shared package download cache
  state/server/<name>/               persistent mutable server data
  states.json                        named external-state registry
~~~

`ATRINIK_WORKSPACE_DIR` relocates the `workspace/` layout but never the primary
component repositories beside the wrapper. Exact root-level component names
and clone-staging directories are ignored by the wrapper repository. Existing
paths are validated as the expected standalone Git repository and are never
overwritten. The top-level workspace, replaceable build directories, and
generated views carry schema-versioned ownership markers. Replacement helpers
require the exact expected marker and verify that the target remains below the
build root.

## Profile resolution and build flow

A profile maps every manifest component to its primary checkout, a managed
worktree label, or an absolute external checkout. Resolution verifies that each
path is a Git worktree whose `origin` or `upstream` identifies the expected
`atrinik/*` repository.

The normalized absolute paths are hashed into the profile build key. A fully
initialized workspace uses the common buildable-component selection so
`build all`, component builds, and launches share incremental output. A partial
workspace uses only the requested target's dependency closure. This separates
worktree combinations while preserving compiler output when the same worktrees
advance. The coordinator creates disposable source views rather than writing
dependency links or output into source checkouts.

The playable build flow is:

~~~text
selected content -> build_runtime.py -> isolated content/lib + content/maps
selected resources -----------------> isolated resource view
selected protocol + library + sound -> client source view -> CMake/Ninja
selected protocol + library --------> server source view -> CMake/Ninja
selected Worker --------------------> npm source view -> npm run check
~~~

The server source view uses a copied `install_data` directory because its CTest
preparation uses CMake directory-copy semantics. Other authored inputs remain
links to their selected worktrees. The Worker view is copied because Node's
module resolver follows configuration-file links and would otherwise search the
component checkout instead of the isolated dependency directory. Collected
content and resource dependency metadata identify the selected path and commit.

## Runtime and state

Server launch preparation assembles a disposable working directory with links
to the selected build, plugins, collected content, resources, configuration,
tools, and named persistent state. First use initializes a state atomically from
the selected server's `install_data`; an existing directory is validated and
never overlaid. State inside a server source worktree is rejected.

The coordinator takes an advisory exclusive lock next to the state directory
before build/runtime preparation and holds it for the lifetime of a launched
server. Profile builds have their own blocking lock, and server launch views are
keyed by state path. Processes started outside the coordinator do not
participate in the state lock, so operators must not point those processes at
the same state concurrently.

## Trust and command execution

Component worktrees are executable source: CMake, Python collection, npm, and
runtime commands execute code from the selected profile. Review a pull request
before selecting its worktree. Profiles do not provide a security sandbox.

Subprocesses use argument arrays rather than a shell. User-provided executable
arguments are not evaluated as shell syntax, and recognized join-password
forms are redacted from diagnostics. Git operations refuse dirty primary
checkouts and dirty managed worktrees where an automatic update or removal
could lose work.

## Release model

Pull-request titles and squash commits use Conventional Commits syntax. A push
to `main` runs semantic-release with the standard conventional-commits
parser: `fix` and other recognized work produce a patch, `feat` produces a
minor, and a breaking change produces a major. The catch-all patch rule ensures
every accepted squash commit produces a release. Releases attach the exact
component manifest and its SHA-256 checksum; component artifacts remain owned
by their respective repositories. The same workflow has a no-input manual
dispatch trigger for recovering a missed or interrupted Actions run; semantic-
release remains responsible for selecting the unreleased commit range and next
version.
