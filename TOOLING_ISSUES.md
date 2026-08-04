# Atrinik tooling issues

This is the cumulative record of development-tool, dependency, build/test
infrastructure, and execution-environment problems observed while working in
this repository. Update an existing entry rather than creating a duplicate.

## Open issues

### TI-025: installed GitHub CLI cannot edit pull requests through GraphQL

- Observed: 2026-08-04
- Area: GitHub CLI / pull-request metadata
- Status: Workaround available
- Symptom: `gh pr edit` fails with a GraphQL error because GitHub has retired
  the Projects Classic `repository.pullRequest.projectCards` field queried by
  the installed GitHub CLI 2.46.0.
- Impact: Pull-request titles and descriptions cannot be updated through the
  normal high-level CLI command even though authentication and API access work.
- Workaround: Update the pull request with `gh api --method PATCH` against the
  REST `repos/{owner}/{repo}/pulls/{number}` endpoint.
- Suggested resolution: Upgrade the GitHub CLI in the Linux build image to a
  version that no longer queries the retired Projects Classic field.

### TI-024: sandboxed GitHub operations cannot use the forwarded SSH agent

- Observed: 2026-08-04
- Area: Git and GitHub CLI / external repository operations
- Status: Workaround available
- Symptom: The devcontainer exposes a VS Code-managed `SSH_AUTH_SOCK`, but
  sandboxed `ssh-add -l` fails with `Operation not permitted`; the same context
  also blocks GitHub DNS resolution. Consequently, sandboxed `git push` and
  GitHub API operations cannot authenticate or reach `github.com`.
- Impact: Explicitly authorized branch pushes and pull-request operations fail
  even though the forwarded agent works from the host execution context.
- Workaround: Run the narrowly scoped `git` and `gh` commands with approved
  host-context execution. Keep source inspection, editing, and local validation
  in the normal sandbox.
- Suggested resolution: Allow explicitly authorized GitHub commands to access
  the forwarded agent socket and outbound GitHub DNS, or document host-context
  execution as the supported workflow for external repository mutations.

### TI-020: Windows image login shells omit MXE compiler drivers from PATH

- Observed: 2026-08-04
- Area: Windows MinGW cross-build validation
- Status: Resolved in image source; publication pending
- Symptom: `ghcr.io/atrinik/windows-build:0.0.2` exposes the MXE toolchain
  variables and its OCI environment contains `/opt/mxe/usr/bin`, but Debian's
  `/etc/profile` replaces `PATH` in login shells. The compiler path selected by
  the MXE toolchain is a ccache symlink, so those shells report
  `Could not find compiler "x86_64-w64-mingw32.shared-gcc" in PATH`.
- Impact: Direct CMake presets and `build-windows.sh` fail during the compiler
  smoke test unless the real MXE driver directory is added to `PATH`.
- Resolution in source: The Windows image now exports an explicit default
  `PATH` and restores the MXE directory from `/etc/profile.d` after login-shell
  initialization. Its build fails unless both the MXE GCC driver and CMake
  wrapper are discoverable as root and `vscode`, including through login
  shells. Pull requests build the image and repeat those checks. Repository
  consumers retain their workaround until the corrected image is published
  and all pins are advanced together.

### TI-019: pinned Linux build image lacked the Clang compiler

- Observed: 2026-08-04
- Area: Native Clang and sanitizer validation
- Status: Resolved in `linux-build:0.0.3`
- Symptom: `ghcr.io/atrinik/linux-build:0.0.2` provides GCC 15, clangd, and
  clang-tidy, but does not provide the `clang` compiler executable or a Clang
  sanitizer toolchain.
- Impact: A Clang ASan/UBSan CI lane cannot run solely with the tools baked
  into the otherwise pinned Linux build image.
- Resolution: The Linux image now installs Clang, compiler-rt, and LLVM and
  verifies the compiler and LLVM tools during the image build. All active
  monorepo Linux pins now use `0.0.3`, and the temporary CI package-install
  workaround has been removed.

### TI-017: sandboxed CMake cannot fetch the pinned server dependency

- Observed: 2026-08-03
- Area: Native C server configuration / FetchContent
- Status: Workaround available
- Symptom: A first `cmake --preset linux-debug` configuration failed after
  three attempts to clone the pinned `libpcpnatpmp` repository because the
  restricted sandbox could not resolve `github.com`.
- Impact: A clean or uncached server build cannot configure in the default
  sandbox, so neither server compilation nor its tests can begin.
- Workaround: Run the initial CMake configure with narrowly approved network
  access. Later configurations reuse the pinned source under the build tree.
- Suggested resolution: Pre-populate the pinned dependency in the published
  Linux build image or provide a repository-controlled dependency cache to
  clean sandboxed builds.

### TI-016: actionlint was unavailable for workflow validation

- Observed: 2026-08-03
- Area: GitHub Actions workflow validation
- Status: Resolved in `linux-build:0.0.3`
- Symptom: The development environment does not provide the `actionlint`
  executable.
- Impact: Workflow edits cannot receive actionlint's expression, event, and
  shell-aware static checks locally.
- Resolution: The Linux image installs the checksum-verified actionlint 1.7.12
  release. The repository's `.github/actionlint.yaml` registers GitHub's newer
  `ubuntu-26.04` hosted runner label until actionlint recognizes it natively.
  The published `linux-build:0.0.3` image now validates the workflows locally.

### TI-015: the standalone Dev Containers CLI was unavailable

- Observed: 2026-08-03
- Area: Devcontainer configuration validation
- Status: Resolved in `linux-build:0.0.3`
- Symptom: `devcontainer read-configuration --workspace-folder .` cannot run
  because the `devcontainer` command is not installed in the agent environment.
- Impact: The resolved image, feature, mount, and user configuration cannot be
  checked locally through the same standalone CLI used by Dev Containers.
- Resolution: The Linux image installs the pinned
  `@devcontainers/cli` 0.88.0 package and verifies its executable and version
  during both the Docker build and image pull-request validation. The published
  image successfully resolves both checked-in devcontainer configurations.

### TI-014: the default ccache directory is read-only in the sandbox

- Observed: 2026-08-03
- Area: Native C client/server builds
- Status: Resolved in `linux-build:0.0.3`
- Symptom: Incremental builds using the configured ccache launcher fail while
  creating entries beneath `/home/ubuntu/.cache/ccache`, which is read-only in
  the restricted execution environment.
- Impact: Otherwise valid client and server builds stop before compilation.
- Resolution in source: The Linux image now defaults `CCACHE_DIR` to
  `/tmp/atrinik-ccache` and creates it with mode 1777. CI can continue to
  override this with a persistent workspace cache.

### TI-011: the Linux devcontainer omitted Python package installation tools

- Observed: 2026-08-03
- Area: Linux development environment / `tools/atrinik_bot`
- Status: Resolved in `linux-build:0.0.3`
- Symptom: `python3 -m pip` failed with `No module named pip` while preparing
  an isolated environment for the bot's QUIC dependency.
- Impact: Python tool dependencies could not be installed into an isolated
  repository virtual environment.
- Workaround: Install `python3-pip` and `python3-venv` in an existing container.
- Resolution: Added both packages to the Linux image Dockerfile owned by
  `atrinik/devcontainer`; the published `0.0.3` image now provides them by
  default.

### TI-010: host-context builds appear as `nobody` in the restricted sandbox

- Observed: 2026-08-03
- Area: Agent execution environment / generated CMake output
- Status: Workaround available
- Symptom: Files created under `build/linux-debug` by an approved host-context
  command appear as owned by `nobody:nogroup` inside the restricted sandbox.
  A later sandboxed build then fails while replacing dependency files with
  `Permission denied`. Sandboxed `sudo` is unavailable because the sandbox
  enables `no-new-privileges`, independently of the devcontainer's normal sudo
  configuration. The pinned Linux validation container can also reject the
  bind-mounted repository as having dubious ownership when a check invokes
  Git. A pinned Windows configure likewise left the source-generated
  `client/src/include/version.h` appearing as `nobody:nogroup`, which blocked
  a later non-root configure from replacing it.
- Impact: Switching between host-context runtime tests and sandboxed builds can
  make an otherwise valid incremental build tree temporarily unwritable.
- Workaround: Remove only the `nobody`-owned generated files beneath the
  affected build directory and reconfigure/rebuild in one execution context.
  Both legacy client and server targets built successfully after regeneration.
  For read-only Git checks in the pinned container, pass a process-local
  `safe.directory` setting through Git's environment-based configuration. A
  fresh Windows validation run can use container root solely to replace the
  affected generated header; it completed successfully with this workaround.
- Suggested resolution: Preserve a consistent UID mapping for workspace files
  across sandbox and approved host executions, or isolate their build output
  directories.

### TI-008: the Linux devcontainer lacked `unzip`

- Observed: 2026-08-03
- Area: Development environment / archive inspection
- Status: Resolved in `linux-build:0.0.3`
- Symptom: Inspecting the official embedded Python ZIP failed with
  `unzip: command not found` in the Linux development container.
- Impact: ZIP package contents cannot be inspected with the conventional CLI
  during dependency and packaging work.
- Resolution in source: The Linux image now installs the distro `unzip`
  package and verifies the executable during the image build. The MXE Windows
  cross-build image was already unaffected.

### TI-023: Git SSH remotes could not be fetched in the development environment

- Observed: 2026-08-04
- Area: Git / remote protocol inspection
- Status: Resolved in `linux-build:0.0.3`; Windows image publication pending
- Symptom: `git fetch zoey` failed with `cannot run ssh: No such file or
  directory` because the configured remote uses an SSH URL but the environment
  has no SSH client executable.
- Impact: The normal named remote could not be refreshed while checking the
  protocol deployed by Zoey's Server.
- Resolution in source: Both build images now install `openssh-client` and
  verify the SSH executable during image validation. The Dev Containers setup
  relies on VS Code's cross-platform forwarding of a running host SSH agent,
  avoiding direct mounts of private key files. HTTPS remains the preferred
  protocol for anonymous read-only access.

### TI-006: repo-local skill scaffolding cannot write `.agents` in the default sandbox

- Observed: 2026-08-03
- Area: Agent execution environment / Codex repo-local skills
- Status: Workaround available
- Symptom: The required `init_skill.py` scaffolder failed while creating
  `.agents/skills` with `[Errno 30] Read-only file system`, even though the
  repository workspace was otherwise writable.
- Impact: New repo-local skills cannot be initialized by the standard
  scaffolder in the default sandbox.
- Workaround: Run the narrowly scoped scaffolder command with approved elevated
  execution, then use the integrated patch action for skill contents. Four
  Atrinik skills were created this way and all passed `quick_validate.py`.
- Suggested resolution: Allow trusted repository sessions to write
  `.agents/skills`, or document that skill initialization requires scoped
  approval while the rest of the workspace remains writable.

### TI-005: installed Uncrustify was incompatible with the repository config

- Observed: 2026-08-03
- Area: C formatting
- Status: Resolved 2026-08-03
- Symptom: A targeted `uncrustify --check` rejects configuration values such
  as `indent_comma_paren = false` as non-numeric and reports
  `align_number_left` as an unknown option.
- Impact: Changed C files could not be checked or formatted with the former
  canonical configuration in the development environment.
- Resolution: The repository migrated to clang-format, reformatted all tracked
  authored C/C++ sources, removed the Uncrustify configuration and helper, and
  added a CI-enforced `bash tools/clang-format.sh --check` entry point. Generated
  and bundled third-party files are listed in `.clang-format-ignore`.

### TI-022: elevated npm installs are not retained for sandboxed follow-up commands

- Observed: 2026-08-03
- Area: Agent execution environment / `metaserver/worker`
- Status: Workaround available
- Symptom: A network-approved `npm ci` reported 97 installed packages, but the
  following sandboxed command saw only broken `node_modules/.bin` links and
  reported `vitest: not found`. The default sandbox also could not resolve the
  npm registry and could not write npm logs under `/home/ubuntu/.npm`.
- Impact: Worker tests cannot be run as a separate sandboxed command after an
  approved dependency installation.
- Workaround: Run `npm ci` and the intended validation command together in the
  same approved execution. The focused regression and complete `npm run check`
  suite passed using this approach.
- Suggested resolution: Preserve approved workspace dependency writes across
  follow-up sandbox calls and provide a writable npm cache/log directory.

### TI-002: sandboxed network tests cannot bind localhost

- Observed: 2026-08-03
- Area: Agent execution environment / local network validation
- Status: Workaround available
- Symptom: `npm --prefix metaserver/worker run check` reaches Vitest but fails
  with `listen EPERM: operation not permitted 127.0.0.1`. Wrangler also cannot
  create its log directory under `/home/ubuntu/.config` in the restricted
  environment. A local Atrinik QUIC server similarly could not bind its
  isolated UDP test port until run with network approval. The Atrinik bot's
  dashboard tests likewise fail with `PermissionError: [Errno 1] Operation not
  permitted` while binding ephemeral `127.0.0.1:0` TCP listeners. Server unit
  fixtures also create dummy-player localhost sockets and fail at the same
  sandbox boundary. The `network-e2e` target likewise reports four errors at
  UDP socket creation; only its socket-free port-mapping scenario runs there.
- Impact: Complete metaserver validation and local client/server transport
  tests cannot run in the default sandbox even though compilation and other
  non-listening steps can run. On 2026-08-03 the bot suite again reached all
  280 non-listening checks but its six web tests failed at listener setup.
- Workaround: Run the check with approved elevated execution. The elevated run
  completed successfully: 16 Vitest tests and 4 Python tests passed, XSD
  validation passed, and the Wrangler deploy dry-run succeeded. The bot's
  elevated full suite most recently passed all 305 tests. The elevated server
  check passed every C suite and all 420 Python plugin tests. The elevated
  `network-e2e` target passed all five identity, QUIC, STUN, hole-punching, and
  port-mapping scenarios.
- Suggested resolution: Keep elevation available for this suite. Setting a
  writable `XDG_CONFIG_HOME` can address Wrangler logging, but localhost bind
  permission is still required.

### TI-003: sandboxed Docker validation needs writable config and elevation

- Observed: 2026-08-03
- Area: Agent execution environment / devcontainer validation
- Status: Workaround available
- Symptom: Docker initially fails with
  `mkdir /home/ubuntu/.docker/buildx: read-only file system`; after redirecting
  its configuration it fails with `permission denied` for
  `/var/run/docker.sock` in the default sandbox. The long-running agent shell
  also remained on the pre-rebuild environment and could not discover
  `miniupnpc` through pkg-config after the rebuilt devcontainer image had
  verified the package.
- Impact: Dockerfile checks, devcontainer image builds, and commands in local
  validation images cannot run with the default agent environment settings.
- Workaround: Set `DOCKER_CONFIG` to a writable directory under `/tmp` and run
  Docker with approved elevated execution. Use the rebuilt server/devcontainer
  images for dependency-sensitive validation when the agent shell is stale.
  Dockerfile checking, complete image builds, and in-image command checks all
  succeeded this way.
- Suggested resolution: Continue providing a writable Docker configuration
  directory and approved daemon access for devcontainer validation.

## Resolved issues

### TI-021: direct MXE toolchain use emitted a deprecation warning

- Observed: 2026-08-04
- Resolved: 2026-08-04
- Area: Windows MinGW configuration
- Status: Resolved
- Symptom: Configuring a Windows preset through the system `cmake` command
  loaded `mxe-conf.cmake` directly and emitted MXE's deprecation warning.
- Impact: Both client and server validation produced avoidable configuration
  warning output and missed MXE's cached run results and policy defaults.
- Resolution: CI, the Windows devcontainer, `build-windows.sh`, and the
  documented commands now configure through
  `x86_64-w64-mingw32.shared-cmake`; build invocations continue to use the
  ordinary CMake build presets.

### TI-018: repeated MinGW server configuration reapplied a non-idempotent patch

- Observed: 2026-08-03
- Resolved: 2026-08-04
- Area: Windows server cross-build / libpcpnatpmp FetchContent
- Status: Resolved
- Symptom: Reconfiguring `windows-mxe-server-release` could rerun the
  `libpcpnatpmp` patch step against an already-patched FetchContent checkout;
  `git apply` then aborted configuration.
- Impact: An existing Windows server build directory was not reliably reusable.
- Resolution: The patch helper now accepts either a clean forward application
  or a clean already-applied reverse check and rejects partial/conflicting
  trees. Two consecutive pinned-image server configurations passed.

### TI-012: the server `proto_unit` generator referenced unavailable tooling

- Observed: 2026-08-03
- Resolved: 2026-08-04
- Area: Server unit-test prototype generation
- Status: Resolved
- Symptom: The target referenced a missing post-processor and legacy `cproto`
  emitted parse diagnostics for current headers.
- Impact: Prototype regeneration failed and produced diagnostic noise.
- Resolution: Stable helper declarations remain in the authored
  `check_proto.h`; suite declarations and dispatch entries are generated from
  the canonical CMake suite manifest. The broken cproto target and dependency
  were removed, and the dedicated test executable validates the declarations.

### TI-013: server test failures did not produce a failing process status

- Observed: 2026-08-03
- Resolved: 2026-08-03
- Area: Server Check and plugin test runners
- Status: Resolved
- Symptom: A deliberately failing focused Check suite printed `Failures: 1`,
  while `atrinik-server --unit` still exited with status 0. Python plugin test
  results were likewise printed but not propagated to the server process.
- Impact: CMake and automation could report success when server tests failed.
- Resolution: The Check runner now accumulates failed-test counts and exits
  nonzero, and the embedded Python runner propagates its success status through
  the plugin event into `atrinik-server --plugin_unit`. The complete aggregate
  target passed every C suite and all 420 Python plugin tests.

### TI-009: the server check target ran from an unprepared build directory

- Observed: 2026-08-03
- Resolved: 2026-08-03
- Area: Server CMake test target / runtime setup
- Status: Resolved
- Symptom: `cmake --build --preset linux-debug --target check` launched the
  server unit binary with the binary directory as its runtime context and
  reported that the data directory was empty. Test initialization could also
  spend time on unnecessary listener and router setup.
- Impact: The documented aggregate target could fail before exercising tests.
- Resolution: The target now runs from `server/`, depends on the server and
  plugin binaries, and disables port mapping. Unit, plugin-test, and world-maker
  modes do not initialize client listeners. CI prepares collected resources,
  runtime data, and plugin links before invoking the target.

### TI-004: `ss` utility was absent

- Observed: 2026-08-03
- Resolved: 2026-08-03
- Area: Linux development environment / server networking diagnostics
- Status: Resolved
- Symptom: Running `ss -lntup` reported `ss: command not found` while checking
  the direct QUIC UDP listener.
- Impact: Developers could not use the documented socket-listener inspection
  to distinguish UDP/QUIC port 1730 from the legacy TCP listeners.
- Previous workaround: Inspect `/proc/net/udp*`, use another network utility,
  or validate the externally published QUIC candidate through the metaserver.
- Resolution: Added `iproute2` to the devcontainer. A complete image build
  passed and `/usr/bin/ss` was present in the resulting image.

### TI-007: devcontainer `ubuntu` user lacked sudo access

- Observed: 2026-08-03
- Resolved: 2026-08-03
- Area: Linux development environment / devcontainer administration
- Status: Resolved
- Symptom: The VS Code devcontainer runs as `ubuntu`, but the image did not
  install or configure `sudo`.
- Impact: Developers could not install temporary diagnostic packages or run
  commands that require root privileges from an interactive devcontainer
  terminal.
- Previous workaround: Rebuild the image after adding every required package
  directly to the former in-repository devcontainer Dockerfile.
- Resolution: Installed the distro `sudo` package, added a mode-0440
  `/etc/sudoers.d/ubuntu` rule granting passwordless sudo, and explicitly
  disabled Docker's `no-new-privileges` security option in devcontainer run
  arguments. A complete image build passed, and running `sudo -n id -u` as
  `ubuntu` with the configured security option returned `0`. The image also
  exposed miniupnpc 2.3.3, libpcpnatpmp 1.0.0, and `/usr/bin/ss`.
  Existing VS Code terminals retain the pre-rebuild filesystem and should be
  reopened after rebuilding the devcontainer before checking these tools.

### TI-001: `file` utility was absent

- Observed: 2026-08-03
- Resolved: 2026-08-03
- Area: Linux development environment
- Status: Resolved
- Symptom: Running `file build/linux-debug/server/atrinik-server` reported
  `file: command not found`.
- Impact: Developers could not use the usual convenience command to identify
  or inspect build artifacts. Compilation was unaffected.
- Previous workaround: Use `readelf -h` for ELF artifacts.
- Resolution: Added the distro `file` package to the Linux image Dockerfile now
  owned by `atrinik/devcontainer`. A complete local devcontainer image build
  passed, and `file --version` inside the resulting image reported `file-5.46`.
