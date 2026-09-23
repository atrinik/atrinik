# Native Linux delivery qualification

This record covers the real native issue-delivery lifecycle for
[issue #593](https://github.com/atrinik/atrinik/issues/593), observed on
2026-09-23 after [PR #592](https://github.com/atrinik/atrinik/pull/592) merged.
It does not complete the separate application and hardware acceptance of
[parent #562](https://github.com/atrinik/atrinik/issues/562).

## Accepted inputs and environment

The accepted wrapper source is
`04042dbeefc6b40d6f906c9344bb76238d852b7c`, merged on 2026-09-23.
Its tracked tree was verified identical to reviewed PR head
`22ca8c90efe960146b8b56329030e8fd0f751200`. The implementation delivery
remained container-bound; this qualification
started a fresh native ledger and did not transfer its ownership or resources.

The observed platform was Ubuntu 26.04.1 LTS, amd64, with systemd, Python
3.14.4, Git 2.53.0, GitHub CLI 2.46.0 and coverage 7.15.3. The public probe
accepted the actual passwd user/home, native filesystem ancestry and private
Codex state. This qualifies the observed configuration; it is not evidence for
other supported distributions or hardware.

Native delivery operations used the same private workspace, Codex home and cache.
`GH_CONFIG_DIR`, `XDG_CONFIG_HOME`, `GH_TOKEN` and `GITHUB_TOKEN` were unset for
the entire delivery shell. No-follow descriptor metadata and ACL checks proved
the standard GitHub store effectively private through its private parent and
credential file. No credential contents were included in evidence or copied.
Fresh actor, repository push, Project and package-read capability checks passed.

## Actual public delivery lifecycle

The unchanged issue skill was explicitly invoked with `ENTRY_MODE=issue` and
`atrinik/atrinik#593`. The following observations came from real public-helper
operations, not fixture substitutes:

| Check | Observed result |
| --- | --- |
| `python3 scripts/atrinik_coordinator_context.py --json` | `native-linux`, `authoritative: true`, no failed checks. |
| Manifest, local and remote worktree/PR inventory | Clean accepted source; no competing issue-593 branch, PR or local ledger. Foreign and uncertain deliveries remained untouched. |
| `init-root`, `inventory`, `prepare`, `create` | Fresh authenticated schema-v1 issue genesis succeeded before branch/worktree or remote delivery mutations. |
| Dedicated wrapper worktree | The documented wrapper-self raw-Git exception used the helper-inspected durable primitive request and its prescribed path rule. |
| `worktree-observe`, `worktree-bind-cas` | `bind-exact`; public observation and atomic binding proved the registered clean branch/head under leases. All subsequent edits and tests used the returned bound path. |
| `revalidate-current-targets-cas` | Clean unchanged targets passed; semantic ownership, target and artifact fields stayed unchanged while generation/history advanced. |
| Fresh-process reconnect | Repeated native context, protected authentication, issue/base, full local ledger inventory, registered clean worktree/head and public neutral CAS successfully. |

The bind and revalidation commands supplied generation, digest and canonical
`--expected-path` from one exact public `inspect` snapshot. A typical
revalidation invocation, from the accepted issue skill directory, was:

```sh
python3 scripts/delivery_ledger.py revalidate-current-targets-cas \
  "$REVIEW_ROOT" "$LEDGER_NAME" \
  --expected-generation "$GENERATION" --expected-digest "$DIGEST" \
  --expected-path "$LEDGER_PATH"
```

These variables denote fresh helper outputs for the already owned delivery,
not permission to adopt an existing path. Reconnect used a new process with the
same genuine identity and private state. The helper acquired and re-proved its
ordered live target leases; saved safety flags were not used as lease proof.
Private recovery records retain exact paths, snapshots and command results.
They are deliberately outside this document and the pull request.

## Supported rejection observations

| Input | Observed result |
| --- | --- |
| Previously consumed generation/digest pair | Exit 2: stale compact CAS tuple with no durable predecessor proof. |
| Incorrect canonical `--expected-path` | Exit 2: stale current-target generation, digest or path. |
| Additional mutable root with unsafe ancestry | Probe exit 2, `unknown-or-unsafe`, `authoritative: false`. |
| Genuine uncommitted qualification document | Exit 2: live Git worktree is dirty; ledger remained unchanged. |

The stale-tuple and wrong-path checks left the canonical ledger digest and
generation unchanged. No authority files, leases or permissions were manually
changed, and no artificial commit or foreign resource was introduced. These
checks do not claim coverage of every possible authentication or lease failure.

## Native wrapper validation

Native wrapper validation uses the bound worktree and an isolated virtual
environment with the repository-pinned coverage dependency. Test-only frontend
wheels were downloaded through the normal PyPI index and checked against its
SHA-256 metadata before installation:

| Test dependency | Wheel SHA-256 |
| --- | --- |
| [CMake 4.2.3](https://pypi.org/project/cmake/4.2.3/) | `8e91b381aaea3c47110583dccc52f4562333d1accdbb806939f953c16e74ec0a` |
| [Ninja 1.13.0](https://pypi.org/project/ninja/1.13.0/) | `fb46acf6b93b8dd0322adc3a4945452a4e774b75b91293bafcc7b7f8e6517dfa` |

Ninja reported `1.13.0.git.kitware.jobserver-pipe-1`. These isolated frontends
satisfy the wrapper's small CMake fixtures; they do not replace the pinned
Classic package lock or install system libraries. The native tool preflight
passed with the private virtual environment on `PATH`.

Delivery operations retained `umask 077`. The test subprocess used `umask 022`
inside its isolated fixtures, with `PYTHONDONTWRITEBYTECODE=1`. Earlier runs
exposed generated-cache permissions, fixture mode assumptions and missing
CMake; their failure logs were preserved. Only this delivery's generated
bytecode directories were made private; tracked source and safety checks were
unchanged. The three affected test methods then passed before the aggregate
rerun.

```sh
# Run in the bound worktree; TEST_VENV is the owned isolated test environment.
(
  umask 022
  export PATH="$TEST_VENV/bin:$PATH" PYTHONDONTWRITEBYTECODE=1
  python3 -m coverage run -m unittest discover -v --durations 50
  python3 -m coverage report --show-missing
)
python3 -m compileall -q atrinik atrinik_workspace tests
python3 -m atrinik_workspace.guidance_inventory --check
./atrinik manifest validate
git diff --check
```

The aggregate rerun passed: **1,735 tests, nine skips**, in 398.839 seconds.
The coverage report completed successfully and reported 85% combined coverage.
Compileall, guidance inventory, manifest validation and whitespace checks all
passed. The skips were one Fish and two Zsh checks (shells unavailable), four
native-Windows checks (different platform), and two real-ccache checks (ccache
unavailable). No skipped check is presented as executed qualification.

## Integration evidence and remaining boundaries

The implementation's merged-source
[Integration validation](https://github.com/atrinik/atrinik/actions/runs/35891854964)
passed separately. Its container and Windows jobs do not establish native
execution on this host.

| Evidence class | Source and limit |
| --- | --- |
| CPU/server and headless lifecycle | PR #592 retained a separately reviewed 54-test server build and an actual ready/clean-stop repeat at wrapper `22ca8c90efe960146b8b56329030e8fd0f751200`. Contemporaneous observations excluded graphics/audio access. This was container evidence, not a native build, client connection or persistence test. |
| Portable export, providers and relocation | [Portable run 34836143288](https://github.com/atrinik/atrinik/actions/runs/34836143288), wrapper `b519a1baa39fe1ae498545acf3db65d84058f01f`, artifact `10344450829`, was independently inspected. Its 2,870 payload files and 84 ELF objects were checked; relocated execution used a network-disabled container without the original source/build paths. This retains its original source coordinate rather than claiming a new native run. |
| Actual media decoding | The same portable run decoded 125 PNG images, loaded eight fonts and produced nonempty PCM from 339 released audio paths. These are actual artifact results, not audible playback or hardware gameplay. |
| Full native Classic toolchain/build | Not qualified here. The host package inventory found 15 exact, 17 missing and nine newer packages against the pinned development lock; CMake and Ninja were absent from the system tool path. The test-only wheels above do not satisfy that full lock. No system package or driver change was performed. Follow [the native toolchain contract](NATIVE_LINUX_TOOLCHAIN.md) on a compatible qualification host. |
| Split client/server authenticated connection and wrong-pin refusal | Outstanding parent acceptance; no successful connection or fingerprint-mismatch result is inferred from a ready server. |
| Persistent player across server restart | Outstanding parent acceptance; temporary-state clean shutdown and external client configuration do not prove saved-player persistence. |
| Selected hardware renderer and gameplay | Outstanding parent acceptance; neither portable relocation nor a software-rendered launch is substitute evidence. |
| Audible playback | Outstanding parent acceptance; PCM decoding and silent sinks do not prove sound was heard. |

The independently inspected portable ZIP SHA-256 was
`ee0607b287d03d56668af236ce58dd090f2f48225220d5996066cc8e42becb20`;
its `client.tar` SHA-256 was
`c3b8e64b3e6d55f978706563328df3f3f7b88c227a9398c343d595a4c80e6b07`.
The producer was
`ghcr.io/atrinik/classic-portable-build@sha256:df72e2ece5edeaee584a1b8eb30e523c6154a0adae7a1fea5e954ed6bc9dbae1`,
with these retained integration source inputs:

| Repository | Revision |
| --- | --- |
| Classic | `4998131ad2ae4c9680685fd87e2d85de1dc15fd9` |
| Content (`main`) | `27c63b969cad8739fd63ed523a33456f7a1595b0` |
| Resources | `bff12778a2181b15c12393b63e4761fd568c8693` |
| Producer | `aa7e944ec3afcf2eaec434b164618680d514a77b` |
| Released sound source | `d0561bf9ff8dc88836818dbe602a5a256c6c0e3f` |

Artifact retention may expire;
these hashes identify retained historical evidence, not a current download or
permission to execute an unverified copy. The fetched Git comparison from the portable run
to the reviewed PR head changed only `tests/test_coordinator_context.py` and
`tests/test_linux_portable_acceptance.py`; production and workflow inputs were
unchanged, and the reviewed head tree matched the merge. These checks support
reuse of that bounded integration evidence, not a new native runtime claim.
Relevant source/input changes require fresh checks before reuse.

This document owns only issue #593's qualification record. Parent #562 remains
open until its independent build, connectivity, persistence, hardware and audio
requirements have fresh evidence and the required deliveries are merged.
