# Bounded MCP source search

`atrinik_workspace.mcp_search` is the search engine behind the local Atrinik
context server. It searches only source snapshots that the wrapper has already
resolved from the canonical component manifest and a configured profile or
worktree selector. A tool request cannot provide a host path, add a repository,
enable network access, or cause an index to persist.

## Host prerequisite

Install `ripgrep` (`rg`) on the Linux host running this adapter. Ubuntu/Debian
hosts can use `sudo apt-get install ripgrep`. The wrapper test-shard workflow
installs it explicitly rather than assuming it is in the hosted runner image.
A missing executable returns `UNSUPPORTED_OPERATION`; it never changes the
source selection or falls back to a broader filesystem scan.

## Request and modes

The engine accepts a closed request object with `mode`, `query`,
`case_sensitive`, `page_size`, `cursor`, and `timeout_ms`. Provenance modes add
the closed fields `provenance`, `path`, and optional `line`. The v1 limits apply:
queries are at most 1,024 characters, a page has at most 50 records, at most
1,000 candidate records participate in one pagination snapshot, and the
deadline is at most 5,000 milliseconds. The whole JSON request remains under
16 KiB. Routine structured results remain under 32 KiB even though the common
hard result ceiling is 64 KiB.

The modes are:

- `exact`: fixed-string source matching through `rg`;
- `regex`: Rust-regex source matching through `rg`;
- `path`: substring matching over the bounded `rg --files` inventory;
- `filename`: substring matching over the final path component; and
- `symbols` and `references`: calls to a language adapter owned by the selected
  physical repository;
- `history`: bounded fixed-string commit-subject history for one current,
  tracked source path; and
- `blame`: bounded line attribution for one current, tracked source path.

There is no generic symbol parser. A symbol or reference request fails with
`UNSUPPORTED_OPERATION` when any selected snapshot lacks its repository-owned
adapter. Search performs no hosted upload, embeddings, network request, shell
evaluation, or persistent cross-worktree indexing.

Git provenance is disabled unless the request sets `provenance` to true. Both
operations pin reads to the snapshot's full commit, reject untracked or unsafe
paths, disable replacement objects, lazy fetching, and ambient system/global
Git config, and return at most 1,000 source records. Missing partial-clone objects
fail locally. `history` accepts an optional fixed-string subject query. `blame`
accepts an optional positive line number and no text query. Ordinary searches
cannot carry provenance fields.

Every Git read reuses the context server's Git read policy. A configured local
clean/process content filter is forbidden before Git inspects source, so a
read-only search cannot execute repository-configured filter programs.

## Resolver boundary and revision identity

The context server resolves user-facing manifest/profile/worktree selectors to
its frozen `Snapshot` interface. Each snapshot contains the physical checkout
root, logical manifest source, full repository coordinate, component metadata,
complete context identity, and `assert_current()` fence. The search engine
consumes that interface directly. The filesystem path is internal server state
and never appears in request or result data.

The coordinate contains repository, branch, full 40-character commit, stable
worktree identity, and optional dirty fingerprint. The engine invokes
`assert_current()` before and after every repository scan and again before
returning, rejecting drift with `STALE_COORDINATE`. It also fences replacement
of a selected root directory.
Cursor identity includes every selected coordinate, component metadata,
manifest identity, effective request parameter, authorization identity digest,
schema version, and provider version. Any change produces `STALE_CURSOR`.

Results repeat the exact source identity on every item. Cross-repository results
therefore remain attributable even though the common result envelope has one
required primary coordinate. Resource links use
`atrinik://OWNER/REPOSITORY/COMMIT/PATH`; a separate bounded resource read must
re-resolve and re-check the same coordinate before returning content.

## Path and content safety

`rg` runs with an argument array and `--no-config` in a minimal environment.
The engine first obtains a sorted bounded inventory through a no-follow
descriptor for the selected logical source root. Content search then gives
ripgrep only descriptor-relative, no-follow regular-file descriptors in small
batches. Each source file is capped at 256 KiB. Stdout and stderr are read
through bounded pipes, and the process is killed on timeout, cancellation, or
output overflow. Search rejects absolute and traversing paths, common-contract
forbidden segments, symlinks in every path component, special or disappearing
files, archives, cache/generated directories, literal-backslash path aliases,
and names that contain secret-assignment forms. Ripgrep's ignore rules remain
active; the common `.git`, `workspace`, `build`, credential, password, secret,
and environment exclusions are enforced again on returned paths. The canonical
source selector also excludes credential-like filenames and suffixes before
inventory admission or opening, including `.env.local`, `credentials.json`,
`secrets.toml`, and `private.key`.

Selected files are also checked against Git in bounded batches. An
`assume-unchanged` or `skip-worktree` index flag invalidates the coordinate. For
a clean snapshot, the bytes of every opened descriptor must hash to the regular
blob recorded for that path at the pinned commit. Dirty snapshots retain their
context-service dirty fingerprint, while still rejecting hidden index flags.
Both clean and dirty snapshots require every selected path to exist in Git's
index; untracked files cannot supply search results.
Clean working-tree transformations whose bytes differ from the pinned blob
(such as CRLF checkout normalization) fail explicitly as
`UNSUPPORTED_OPERATION`; the search layer does not run clean/smudge filters.

Source lines are untrusted data. Snippets are capped at 512 UTF-8 bytes and
common secret assignments are redacted before serialization. Error messages do
not include the query, path, source bytes, ripgrep diagnostics, adapter output,
authorization identity, or host root. Malformed adapter records and unsafe
paths fail closed.

## Pagination and incomplete results

Records are sorted by canonical JSON bytes, so pagination is deterministic and
duplicate-free for one complete snapshot identity. A page shortened to meet
the 32 KiB routine budget advances its cursor only past records actually
returned. Reaching the scan/output cap sets `incomplete`, the applicable
truncation flag, and one bounded `INCOMPLETE` failure rather than starting an
unbounded rescan. Cancellation and timeout are checked between records as well
as while a subprocess is producing output.

The tests use synthetic Git repositories. Run the fixture suite with:

```sh
python3 -m unittest -v tests.test_mcp_search
```
