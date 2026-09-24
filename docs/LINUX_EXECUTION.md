# Linux execution and authority

Use the accepted execution probe and delivery helpers unchanged. A proposed
patch, fixture result, environment flag or issue description cannot authorize
its own delivery or switch an existing delivery's execution context.

| Host / role | Display | Delivery authority | Evidence |
| --- | --- | --- | --- |
| Ubuntu 24.04/26.04 or Debian 12/13, direct systemd host or VM | Optional | `native-linux` after complete live proof | Distribution-specific native tests required |
| Short-lived pinned Linux build worker | None required | No delivery authority | Exact image, source, cache and resource proofs |
| Already-bound historical Linux coordinator | None required | Existing `canonical-linux` contract after complete live proof | Exact retained image/mount/worktree/ledger coordinates |
| Optional native Linux client container | Selected X11/Wayland and real GPU | Runtime capability does not grant delivery authority | Selected renderer and gameplay evidence |
| Windows/WSL2/WSLg desktop runtime | WSLg in explicit desktop config | Runtime capability grants no delivery authority | Keep WSLg and native Windows evidence separate |
| Windows cross-build container | None | No ledger authority | MXE package/build evidence only |
| Native Windows | Native graphics for D3D12 qualification | No Linux ledger authority | Native Windows runtime evidence |

The version list is the implementation's supported systemd distribution contract,
not a claim that every version has hardware or integration qualification. Record
actual test coverage separately; never infer Debian or older Ubuntu coverage from
the pinned Ubuntu image or synthetic fixtures.

The direct-host probe reads bounded trusted OS files through no-follow descriptor
traversal, accepts the actual passwd home/user instead of a fixed UID, and requires
private user-owned Codex state. Repository, home, Codex and existing mutable roots
must use native ext4, XFS, Btrfs, ZFS or tmpfs with trusted owner/mode ancestry.
World/group-writable ancestors, links, network/bridge filesystems and missing
proof fail closed. The probe checks `ATRINIK_WORKSPACE_DIR`, its generated build
and runtime directories, and additional exact `--mutable-root PATH` subjects.
Missing descendants require proof of their nearest existing no-follow ancestor.
The delivery helper independently derives storage, resource, scope, review and
lease subjects from retained records, then uses the same trusted context code
before operational admission and again under leases before publication. File
subjects such as profiles and locks include their own filesystem mount. Operator
probe arguments cannot substitute for helper-derived ownership or permission.
Existing review-lock bookkeeping remains separate from operational admission;
missing descendants grant no reuse or creation authority.
Project delivery also proves the resolved primary checkout build directory and
its exact project directory, record and lock before storage access, then repeats
that proof inside the existing transaction before publication. Linked-worktree
invocation does not exempt the primary storage filesystem. Native Windows emits
its stable non-authoritative diagnostic without loading Linux authority source.

Both caller and PID1 UID/GID maps must contain the full initial identity row
`0 0 4294967295`. Caller numeric-PID mountinfo must identify genuine procfs
and supported filesystems at the checked paths. PID1 must report root UID and systemd; installed
systemd and `/run/systemd/system` must be trusted, root-owned and non-writable
by other users. WSL and container signals reject direct-native classification.
Ordinary-user denial of PID1 executable/namespace links is not a request for
privilege escalation and does not itself reject a supported host.

Trust includes the kernel, host administrator and executing user/code. This
recognizes an ordinary supported host configuration; it does not attest PID1
executable bytes, resist privileged host administration, or distinguish every
administrator-created container/chroot. Initial UID/GID mapping checks reject
ordinary rootless ownership forgery. Environment flags and systemd names alone
never establish authority.

After context proof, unchanged authenticated issue/PR ownership, ignored review
root inventory, dedicated safe worktree binding, no-follow Git/source proof,
CAS, ordered leases, collision checks and recovery rules all still apply.
Parallel sessions isolate Codex/cache/worktree/state coordinates. Existing
canonical containers continue without replacement or remount.

Before building source in a selected environment, run
`python3 -m atrinik_workspace.linux_platform` there for a bounded report of
build tools and Git LFS filters. Add `--docker` only when that build or runtime
operation needs Docker. The report diagnoses the selected build environment; it
does not grant `native-linux` or `canonical-linux` authority and is not required
to verify or run an exported client. Missing Docker permission is independent of
graphics availability. Tool presence is not proof of complete build
compatibility; run the owner-required build and test checks in that environment.


## Native development with pinned CPU build workers

Supported native Linux normally runs editing, Git, delivery helpers, review and
lightweight Python/guidance checks locally in its bound worktree. Application
builds and toolchain-dependent tests use the already cached immutable Linux
image. No mandatory host compiler, QEMU, GUI editor or long-lived coordinator
container is needed. An already-bound historical container delivery retains
its original coordinates under the compatibility contract below.

A build worker is an execution resource, not delivery authority. Keep native
`HOME`, private Codex state, authentication and evidence on the host. Native
ownership survives worker exit/recreation, while each new execution still
requires the exact source, input, resource and lease checks. The stock
`.devcontainer/devcontainer.json` is a development coordinator composition: it
mounts auth/Codex state, runs initialization and uses `/workspaces/atrinik`.
It is not a drop-in credential-free worker for a native linked worktree.

For a fresh owned source namespace, use this same-absolute-path composition.
Obtain `NATIVE_PRIMARY` and `NATIVE_WORKTREE` from the accepted wrapper worktree
inventory/binding; never infer them from a container name. Initialize the bound
worktree's default workspace with `./atrinik status --json` locally first. All
selected component checkouts must be inside this owned namespace; if the profile
selects an external checkout/common Git directory, explicitly inventory and mount
that exact source and lease namespace at its unchanged absolute path too. Do not
silently copy sources, rewrite gitfiles or substitute another workspace.

Before exposing the namespace, inspect its readable source and Git configuration
for credentials. Keep native Codex/auth/evidence as private siblings outside it.
Keep each actual `build/reviews` operational directory visible read-only at its
unchanged path and inode, including its ledger, receipts, locks and required
nonsecret report. The helper locks that directory itself; its live inventory
protects retained resource reservations during builds. Never hide, copy or mask
these roots: an invisible root can appear absent and bypass resource protection.
Use a minimal nonsecret operational report and retain rich private evidence
outside the namespace. An empty environment alone does not prevent reading
mounted credentials. Stop if an existing namespace contains private/unrelated
reports or other files that cannot safely be exposed to this trusted worker.

```bash
# Run on the native host, after live ownership and resource planning.
# These two values are the exact returned, owned coordinates, not examples to adopt.
NATIVE_PRIMARY=/absolute/owned/primary
NATIVE_WORKTREE=/absolute/owned/primary/workspace/worktrees/atrinik/owned-label
BUILD_JOB=atrinik-owned-unique-build
BUILD_IMAGE=ghcr.io/atrinik/linux-build:1.10.0@sha256:7904a1802054662b0ede5b55de72e4c92b0112a3c211125f994ed6c62e9ec9d8
COMMON_GIT=$(git -C "$NATIVE_WORKTREE" rev-parse --path-format=absolute --git-common-dir)
# The wrapper status operation has established this physical lease namespace.
BUILD_LEASES="$COMMON_GIT/atrinik-resource-leases"
test -d "$BUILD_LEASES"
docker image inspect "$BUILD_IMAGE" >/dev/null
BUILD_ARGS=(--init --pull never --user "$(id -u):$(id -g)"
  --read-only --cap-drop ALL --security-opt no-new-privileges
  --tmpfs /tmp:rw,exec,nosuid,nodev,mode=1777
  --mount "type=bind,source=$NATIVE_PRIMARY,target=$NATIVE_PRIMARY,readonly"
  --mount "type=bind,source=$NATIVE_WORKTREE,target=$NATIVE_WORKTREE"
  --mount "type=bind,source=$COMMON_GIT,target=$COMMON_GIT,readonly"
  --mount "type=bind,source=$BUILD_LEASES,target=$BUILD_LEASES"
  --workdir "$NATIVE_WORKTREE")
# Preserve real operational directory locks and atomic ledger replacements.
for REVIEW_ROOT in "$NATIVE_PRIMARY/build/reviews" "$NATIVE_WORKTREE/build/reviews"; do
  if [ -d "$REVIEW_ROOT" ]; then
    BUILD_ARGS+=(--mount "type=bind,source=$REVIEW_ROOT,target=$REVIEW_ROOT,readonly")
  fi
done
# Missing roots must be genuinely absent. Do not create/hide them for this recipe.
# Create, inspect, then start each exact owned worker. No auth, Codex, Docker
# socket, display, GPU or audio mounts; no host environment passthrough.
PLAN_ID=$(docker create "${BUILD_ARGS[@]}" --name "$BUILD_JOB-plan" "$BUILD_IMAGE" \
  bash -c 'umask 077; ./atrinik build server --profile classic --test --plan --json')
docker inspect "$PLAN_ID" --format '{{.Id}} {{.Image}} {{json .Mounts}} {{json .HostConfig.Tmpfs}}'
# Verify the exact image and every same-path mount before starting.
docker start --attach "$PLAN_ID"
docker inspect "$PLAN_ID" --format '{{.State.Status}} {{.State.ExitCode}}'
```

Retain raw successful plan stdout outside the mounted namespace. Require stopped
status and exit code zero; `docker start --attach` alone is not build acceptance.
The actual plan supplies the build/source-generation paths and `plan_sha256`.
Record the required resource intent through the native delivery helper before
executing. With the same `BUILD_ARGS`, image, worktree, profile and build options,
create a second uniquely named worker whose command is:

```bash
./atrinik build server --profile classic --test --expected-plan RETURNED_PLAN_SHA256
```

Inspect that worker's exact image/mounts before starting it, retain its output
and require stopped status/zero exit code. Run the build-environment preflight
`python3 -m atrinik_workspace.linux_platform` in this same composition when
selecting it for source builds. Plan and execute in the selected container build
environment, never plan with a different host toolchain. Use the owner-required
component/profile and tests for the actual task. Package fetches may need network
access; registry login remains a host action and no credential store is mounted.

The bound worktree's persistent `workspace/build` holds its build outputs and
wrapper-managed compiler/dependency caches. Its stable owner/path survives
short-lived workers; image layers are also reused through `--pull never`.
Keep the same image/toolchain, source/input identities, profile and lease paths.
The wrapper's build-plan fencing, source generations, cache keys, metadata and
compatibility checks remain required: a cache hit never proves acceptance.
Distinct concurrent owners use distinct mutable build/cache roots; do not share
a writable cache merely because image tags match. If a separately owned volume
is used, mount it at the exact recorded build path and retain its identity across
workers, without overlaying occupied state or changing Git/lease paths.

The private temporary filesystem permits execution because compiler probes and
test fixtures run generated programs there; Docker tmpfs defaults otherwise
can block these checks. Keep `nosuid,nodev` and the unprivileged user.

Before execution, compare public helper inventory inside the worker with native
inventory and verify the real review directory inode and shared lock behavior.
Keep the directory bind across atomic ledger updates; do not bind individual
ledger files or substitute a saved inventory. Build admission still re-inventories
reservations at the point of use under its leases. A native preflight never
substitutes for that worker-side check.

Preserve stopped worker identities, logs and failed output required by the
resource lifecycle. Stop only an exact owned running worker with a bounded
`docker stop --time 20 WORKER_ID` after checking its active holders. Do not use
`--rm`, prune caches, remove volumes or invoke cleanup as part of normal worker
exit. Cleanup remains separately authorized and preview-first. Reconnect still
requires fresh native context, actor, inventory, clean worktree, CAS and lease
proof; this workflow grants no dirty-work adoption or ledger rewriting.

## Optional direct-host source builds and native terminal selection

Native authority eligibility on the supported Debian/Ubuntu systemd
distributions does not require a compiler, development headers, the exact Ubuntu
package lock or a successful build preflight. Authority comes from the separate
live host/context, authenticated ownership, worktree, ledger and lease proofs
above. A verified exported client likewise needs runtime loader, display,
graphics and audio capabilities rather than a source toolchain.

The default application build and build-test path is the short-lived pinned
CPU worker described above. If a developer deliberately chooses a direct-host
source build, install the wrapper and source-build front-end dependencies through
the distribution package manager:

```sh
sudo apt-get update
sudo apt-get install python3 python3-venv git git-lfs cmake ninja-build pkg-config
git lfs install --skip-repo
python3 -m atrinik_workspace.linux_platform
```

For complete Classic client/server/test prerequisites, follow the
[optional Ubuntu 26.04 native toolchain recipe](NATIVE_LINUX_TOOLCHAIN.md): exact
owner package snapshot, user-local pinned audio libraries, wrapper-managed
shader tools, integrated tests and headless runtime commands. Its source
contract is container-tested; direct-host qualification is separately recorded
under #593. The authority distribution list alone does not establish build
qualification. This optional recipe is not needed merely to establish native
authority or run an exported client. Mesa hosts need the distribution's
`mesa-vulkan-drivers`; NVIDIA hosts need the installed proprietary driver's
matching Vulkan ICD. Preserve a working host driver. The portable image contains
the Vulkan loader, not graphics drivers. Native client execution does not require
Docker; a client container additionally requires a working daemon and, for
NVIDIA, the NVIDIA Container Toolkit.

Use the actual passwd user's private home/Codex directory and the standard GitHub credential
store described in [coordinator authentication](COORDINATOR_AUTH.md). Run the
public coordinator probe before issue/project preparation. Reconnect to the same
host, user, worktree and ledger; rerun live context, authenticated actor,
complete collision inventory and the helper's target/CAS/lease proof. A saved
success document never authorizes reconnect. During delivery of a change to this
contract, keep that delivery's previously accepted execution context.

## Build and runtime container prerequisites

Install Docker Engine using the distribution's official instructions for
[Ubuntu](https://docs.docker.com/engine/install/ubuntu/) or
[Debian](https://docs.docker.com/engine/install/debian/). An existing installation
and its resources must be preserved. Configure daemon access through the host
administrator's chosen supported method; a socket permission failure is a Docker
access problem, independent of graphics. For the explicit server-runtime,
WSLg or cross-build configurations, install Node.js/npm and the
[Dev Containers CLI](https://code.visualstudio.com/docs/devcontainers/devcontainer-cli)
in the host user's tool environment, then verify both interfaces:

```sh
npm install --global --prefix "$HOME/.local" @devcontainers/cli
export PATH="$HOME/.local/bin:$PATH"
devcontainer --version
docker version
```

The pinned Atrinik images require the appropriate GHCR package read access when
private. GitHub CLI authentication for coordinator operations does not log Docker
into a registry. The host owner establishes Docker authentication separately;
reuse an existing working Docker login. For a missing login, use a host credential
helper and GitHub's [container registry authentication procedure](https://docs.github.com/en/packages/working-with-a-github-packages-registry/working-with-the-container-registry).
The following command prompts privately for the host owner's classic PAT with
`read:packages` and any required organization SSO authorization. Replace the login
placeholder with the account that has access; never put the token in argv, source,
logs or a worker environment. Agents do not perform this login or copy the mounted
GitHub CLI token into Docker configuration.

```sh
docker login ghcr.io --username YOUR_GITHUB_LOGIN
docker pull ghcr.io/atrinik/linux-build:1.10.0@sha256:7904a1802054662b0ede5b55de72e4c92b0112a3c211125f994ed6c62e9ec9d8
```

For portable export, pull its separate producer only when needed:

```sh
docker pull ghcr.io/atrinik/classic-portable-build@sha256:df72e2ece5edeaee584a1b8eb30e523c6154a0adae7a1fea5e954ed6bc9dbae1
```

A pull of a public package needs no login. A private-package denial requires the
host owner to correct that package's access; it is not permission to make a
package public or pass credentials into the runtime. Keep native authentication
and historical read-only coordinator binds within [COORDINATOR_AUTH.md](COORDINATOR_AUTH.md);
never mount the host Docker credential store into build/runtime workers.
For short-lived CPU workers, use the composition above with Docker `--init`.
Inspect the exact worker's image, mounts and `HostConfig.Init` before starting.
The small init process forwards signals and reaps orphaned descendants; it
never replaces wrapper ownership, supervised shutdown or final holder checks.
The server-runtime, WSLg and cross-build configurations retain their own
creation and capability contracts below. Configuration parsing alone does not
prove a fresh worker's lifecycle; retain actual start/exit evidence.

## Existing bound container compatibility

An already-bound historical canonical delivery may continue only at its exact
owner/container/image/configured-mount/worktree/ledger coordinates. Reconnect
re-proves the live context, authenticated actor, complete inventory, clean
worktree, public CAS and ordered leases. Its existing read-only host GitHub auth
bind remains subject to [the authentication contract](COORDINATOR_AUTH.md).
Names, `entry_mode` and copied or stale records are corroboration only.

Preserve the worktree, ledger, caches, resources and original creation settings;
never replace, remount, transfer or adopt the delivery implicitly. Existing
idle/lifetime bounds remain 30 minutes/12 hours, with active holders checked
before stopping only owned resources. Attaching or restarting does not add
`--init`, ports or mounts. A failed probe or missing identity stops reuse; it
never authorizes a new container-development setup. Codex never launches or
controls VS Code, its executable/URI or GUI automation. No nested coordinator
or credential/key copying is permitted.

## Explicit Linux desktop container capabilities

On the selected Linux desktop, the standalone module prints a JSON Docker
argument array without launching anything. Choose the active session and a
render node discovered on that host, for example:

```sh
python3 -m atrinik_workspace.linux_platform --desktop x11 --gpu mesa --render-device /dev/dri/renderD128 --audio
python3 -m atrinik_workspace.linux_platform --desktop x11 --gpu nvidia --audio
```

The render-node number above is an example, not a fixed assignment. The portable
client uses X11; on a Wayland desktop select its actual XWayland `DISPLAY` and
`XAUTHORITY`. The separate `--desktop wayland` capability is only for a client
that actually implements that backend. X11 requires
the actual private `XAUTHORITY` file and local `DISPLAY`; Wayland requires the
active `XDG_RUNTIME_DIR` and `WAYLAND_DISPLAY`. Audio is an explicit Pulse socket
option, including PipeWire's Pulse compatibility service where present. The
output uses the caller's actual UID/GID and selected device's group, read-only
session socket/cookie mounts, and no global X-server access grant. X11 also sets
the container hostname to the observed desktop hostname so FamilyLocal cookie
records select the same hostname/display rather than Docker's generated name. A selected
NVIDIA runtime additionally needs the host NVIDIA Container Toolkit and a
compatible driver; emitted arguments do not establish that those are installed.

Check the process status before parsing stdout. Desktop failure returns 2 with
an error on stderr and no success document. Pass successful `docker_arguments`
as an argument array to a Docker invocation, never through shell `eval`. Add the
accepted immutable image, isolated client configuration/state and explicit
server endpoint through the runtime workflow. Run `--docker` separately to
check daemon/user permission; that check says nothing about display or GPU.
Actual selected-renderer evidence and interactive gameplay remain required.

For NVIDIA client containers, the host administrator follows the official
[NVIDIA Container Toolkit installation guide](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html).
On a fresh Debian/Ubuntu Docker host, the current stable package setup is:

```sh
sudo apt-get update
sudo apt-get install -y --no-install-recommends ca-certificates curl gnupg2
curl -fsSL https://nvidia.github.io/libnvidia-container/gpgkey | \
  sudo gpg --dearmor -o /usr/share/keyrings/nvidia-container-toolkit-keyring.gpg
curl -fsSL https://nvidia.github.io/libnvidia-container/stable/deb/nvidia-container-toolkit.list | \
  sed 's#deb https://#deb [signed-by=/usr/share/keyrings/nvidia-container-toolkit-keyring.gpg] https://#g' | \
  sudo tee /etc/apt/sources.list.d/nvidia-container-toolkit.list
sudo apt-get update
NVIDIA_CONTAINER_TOOLKIT_VERSION=1.20.0-1
sudo apt-get install -y \
  nvidia-container-toolkit="$NVIDIA_CONTAINER_TOOLKIT_VERSION" \
  nvidia-container-toolkit-base="$NVIDIA_CONTAINER_TOOLKIT_VERSION" \
  libnvidia-container-tools="$NVIDIA_CONTAINER_TOOLKIT_VERSION" \
  libnvidia-container1="$NVIDIA_CONTAINER_TOOLKIT_VERSION"
sudo nvidia-ctk runtime configure --runtime=docker
```

Restart Docker only after the owners of its active containers have stopped their
work and the host administrator has selected that maintenance window:
`sudo systemctl restart docker`. Do not restart it as a worker workaround.
Diagnose with `nvidia-smi`, `nvidia-ctk --version`,
`docker info --format '{{json .Runtimes}}'`, and the module's separate `--docker`
probe. These are capability diagnostics, not selected-renderer/gameplay proof.
Existing working Toolkit/driver installations need no reinstallation.

This complete terminal launcher takes a verified exported directory, explicit
private config directory and authenticated server tuple. Run it on the desktop
host from the wrapper checkout. Set `GPU_KIND=mesa` and the discovered `RENDER_DEVICE`
for Mesa, or `GPU_KIND=nvidia` for the configured NVIDIA runtime. `WITH_AUDIO=1`
opts in to the actual Pulse socket. Use a unique owned container name:

```sh
export CLIENT_EXPORT=/absolute/path/to/verified/client
export CLIENT_CONFIG="$HOME/.local/state/atrinik-client/container-review"
export CLIENT_CONTAINER=atrinik-client-review
export SERVER_HOST=127.0.0.1 SERVER_PORT=17300 SERVER_FINGERPRINT=THE_VERIFIED_64_HEX_FINGERPRINT
export GPU_KIND=nvidia WITH_AUDIO=1
python3 - <<'CLIENT'
import json, os, re, subprocess
from pathlib import Path
payload = Path(os.environ['CLIENT_EXPORT']).resolve(strict=True)
config = Path(os.environ['CLIENT_CONFIG']).expanduser()
config.mkdir(parents=True, mode=0o700, exist_ok=True)
if config.is_symlink() or config.stat().st_uid != os.getuid() or config.stat().st_mode & 0o077:
    raise SystemExit('client config must be private and user-owned')
port = int(os.environ['SERVER_PORT'])
fingerprint = os.environ['SERVER_FINGERPRINT']
host = os.environ['SERVER_HOST']
name = os.environ['CLIENT_CONTAINER']
if not 1 <= port <= 65535 or not re.fullmatch('[0-9a-f]{64}', fingerprint):
    raise SystemExit('invalid authenticated endpoint')
if not re.fullmatch('[A-Za-z0-9_.:-]+', host) or not re.fullmatch('[A-Za-z0-9][A-Za-z0-9_.-]+', name):
    raise SystemExit('invalid host or container name')
probe = ['python3', '-m', 'atrinik_workspace.linux_platform', '--desktop', 'x11', '--gpu', os.environ['GPU_KIND']]
if os.environ['GPU_KIND'] == 'mesa':
    probe += ['--render-device', os.environ['RENDER_DEVICE']]
if os.environ.get('WITH_AUDIO') == '1':
    probe += ['--audio']
result = json.loads(subprocess.run(probe, check=True, text=True, capture_output=True).stdout)
# Host networking makes the explicitly selected same-host UDP mapping reachable;
# it is a runtime capability, never coordinator authority.
arguments = ['docker', 'run', '--detach', '--init', '--name', name, '--network', 'host',
    '--read-only', '--cap-drop', 'ALL', '--security-opt', 'no-new-privileges',
    '--tmpfs', '/tmp:rw,nosuid,nodev,mode=1777',
    *result['docker_arguments'],
    '--mount', f'type=bind,source={payload},target=/client,readonly',
    '--mount', f'type=bind,source={config},target=/client-state',
    '--env', 'ATRINIK_CONFIG_DIR=/client-state', '--workdir', '/client',
    'ghcr.io/atrinik/linux-build:1.10.0@sha256:7904a1802054662b0ede5b55de72e4c92b0112a3c211125f994ed6c62e9ec9d8',
    '/client/atrinik', f'--server={host} {port} {fingerprint}', '--stun_server=off', '--nometa']
container_id = subprocess.run(arguments, check=True, text=True, capture_output=True).stdout.strip()
if not re.fullmatch('[0-9a-f]{64}', container_id):
    raise SystemExit('missing exact container identity; inspect the owned name before any retry')
print(container_id)
CLIENT
```

Preserve the returned full ID as `CLIENT_CONTAINER_ID`. Use
`docker logs --tail 100 "$CLIENT_CONTAINER_ID"` for diagnostics and
`docker attach --sig-proxy=false "$CLIENT_CONTAINER_ID"` for terminal attachment;
GUI interaction stays in the selected desktop session. Exit the game normally,
then `docker wait "$CLIENT_CONTAINER_ID"` and inspect its stopped state. If bounded
shutdown is required, stop only that verified owned ID with
`docker stop --time 20 "$CLIENT_CONTAINER_ID"`. This grants no container/volume
removal or cleanup. Never pass shell `eval`, global `xhost` access, privileged
mode, host credentials or a Docker socket to the client.

## Portable client export

The public exporter runs in the exact published portable build environment,
`ghcr.io/atrinik/classic-portable-build@sha256:df72e2ece5edeaee584a1b8eb30e523c6154a0adae7a1fea5e954ed6bc9dbae1`.
This is a separate immutable producer after the ordinary pinned devcontainer
build/test stage; it is not a substitute coordinator, native authority probe or
host runtime. Before merge, the automatic
`Linux portable acceptance` pull-request workflow is the supported actual
producer route; the retained canonical container has no Docker bridge. Its
`Portable client build and relocation` job is nonpublishing, limits the producer
to two CPUs, and serializes heavy runs without cancelling another owner's run.
The `linux-portable-HEAD_SHA` artifact retains the movable client, exact source
and release identities, configure/compiler evidence and relocation results for
seven days. Download and retain evidence needed for the delivery before expiry.
The final runtime process has neither credentials nor original source/build
mounts, and its network, display, GPU and audio endpoints are absent.

Its current full-commit consumer
guard requires Classic `4998131ad2ae4c9680685fd87e2d85de1dc15fd9`.
Use a clean Classic profile with verified released sound matching its selected
sound source commit, and a new absolute destination
whose parent is owned by the invoking user and not writable by other users.
Create `classic-released-audio` with the exact v1.0.0 coordinates in the
[released sound recipe](../README.md) before running these producer
commands (the CI driver creates its own equivalent saved profile):

```sh
mkdir -p "$HOME/.local/opt"
chmod 700 "$HOME/.local/opt"
./atrinik linux export --profile classic-released-audio --output "$HOME/.local/opt/atrinik-client"
./atrinik linux verify "$HOME/.local/opt/atrinik-client"
```

Both commands emit one JSON result on success and route diagnostics to stderr.
Export refuses an occupied destination. It verifies immutable Git/LFS source
payloads under the existing profile/source leases, uses a separate producer-bound
build cache, and retains the build lease through source revalidation and atomic
publication. A failed operation preserves its private `.atrinik-export-*` staging
for diagnosis and does not replace an existing output.

The complete installed producer closure is bound to an independently reconstructed
OCI overlay inventory: 299 regular files, including all eight upstream archives,
222 Debian source archives, build recipes and notices. Three pinned aggregate
SHA-256 values cover the producer tree, 145 Debian copyright paths and 14 common
license texts. Each canonical JSON map uses sorted keys and compact separators;
producer/common-license keys are relative paths and values are
`sha256`, `size`, `executable`. Debian keys are requested absolute copyright paths
and values additionally contain the resolved absolute path. Missing, extra,
nonregular, changed or mode-different producer files reject publication; source
copies must retain the exact checked inventory. Hashes change only with a newly
qualified immutable producer, never to accept local corruption.

The directory contains the client, materialized media, application libraries,
OpenSSL provider, corresponding sources, producer recipes and notices. ELF
inspection checks actual provider hashes, static dependencies, symbol/version
providers and declared dynamic SONAMEs. The host supplies glibc 2.36 or newer,
its loader and graphics drivers. The X11/Vulkan producer uses XWayland on a
Wayland desktop; PulseAudio or PipeWire's Pulse service supplies audio.

The pinned Debian libpulse input has an absolute PulseAudio search directory.
Export derives only that exact provider's private copy by replacing its 37-byte
NUL-terminated DT_RUNPATH string with `$ORIGIN` and NUL padding. Strict ELF
inspection remains mandatory for the derived bytes and every other provider.
`portable-evidence.json` records the original and exported hashes, immutable image,
provider path and exact transformation. The standalone
`sources/export-recipes/libpulse-origin.py` recipe reproduces the derivative from
the original library; corresponding sources and a changed-library notice remain
included. The installed producer is unchanged and shared libraries stay replaceable.

Portable CI writes validated inputs before export and retains bounded stdout,
stderr, exit/timeout metadata and available exact-profile compiler/client ELF
diagnostics on failure. Failure produces no successful export report or archive;
a secondary collector failure does not mask the export's original failure.

Move the whole directory, then run its `atrinik` launcher. It sets library and
OpenSSL-provider paths relative to itself and keeps mutable configuration outside
the output. Choose `ATRINIK_CONFIG_DIR` explicitly for isolated clients.
`linux verify` checks exact bytes, modes and inventory after relocation; actual
loader, media-decoding, connectivity and per-host qualification remain separate
acceptance results. Hardware gameplay and audible playback need their own proof.
Neither `linux verify` nor launcher execution requires the host source-build
package lock, compiler or `linux_platform` build-tool report. They do require
the verified complete payload and the native runtime capabilities used by the
selected test.

## Independent headless server and native client

Use an isolated server workspace and container, with no display/audio mounts or
GPU device. Publish its chosen UDP port at container creation; attach/reconnect
cannot add a Docker mapping. For a same-host desktop, the mapping is
`--publish 127.0.0.1:17300:17300/udp`. A remote desktop requires an explicitly
chosen reachable address and corresponding firewall rule. A localhost mapping
must not be advertised as remotely reachable.

Use the distinct credential-free runtime configuration in an independently owned
server checkout. It has no Codex directory, GitHub credential store, Git config,
private key, display/audio socket or GPU mount. Its only additional mount is its
own uniquely named build cache. Host Docker authentication may pull the pinned
image before creation; that authentication is never mounted into the runtime.

```sh
HOST_REPO=/absolute/path/to/the/reserved/server-checkout
devcontainer up --workspace-folder "$HOST_REPO" \
  --config "$HOST_REPO/.devcontainer/server-runtime.json"
```

Capture the exact returned full container ID as `SERVER_CONTAINER_ID`, inspect
its image/mounts/published UDP port, and use `docker exec --user ubuntu --workdir
/workspaces/atrinik "$SERVER_CONTAINER_ID" ...` for the following server commands.
This is an execution worker, not a delivery coordinator: do not run ledger or
project helpers there or infer authority from its pinned build image. An existing
container keeps its creation settings and must not be replaced or remounted to
apply this configuration. Container DNS/loopback is its own namespace; the native
desktop in this same Docker host's network namespace reaches the published host
`127.0.0.1`, while another container requires an independently verified route.

Inside that owned container, prepare and start only the server with registered
persistent state. Select `all-ipv4` in both the public plan and launch so the
server accepts Docker-forwarded UDP on its container interface. The default
`loopback` listener only accepts traffic on the container's own loopback;
Docker publication alone does not change it. This container listener does not
change the localhost-only host publication or advertise a wildcard destination.
No host networking, privilege, client, display or audio device is needed:

```sh
./atrinik init classic-server content resources --jobs 2
./atrinik state add linux-review
./atrinik topology show classic --state linux-review --service server \
  --server-listener all-ipv4 --json
./atrinik up --name linux-headless --profile classic \
  --state linux-review --service server --port 17300 --server-listener all-ipv4
./atrinik ps linux-headless --json
./atrinik logs linux-headless server --tail 100
```

Wait for the exact topology's ready status. Hand off only `endpoint.port` and
`endpoint.fingerprint` from its current `ps --json` result plus the reachable
host address. The fingerprint is the 64-hex SHA-256 certificate identity; never
copy the private QUIC key, supervisor control socket, account password or state
files into a public artifact.

Move and verify the client export on the desktop host. Use a private mutable
configuration outside the export and pass the complete authenticated QUIC tuple
as one argument:

```sh
install -d -m 700 "$HOME/.local/state/atrinik-client/linux-review"
ATRINIK_CONFIG_DIR="$HOME/.local/state/atrinik-client/linux-review" \
  /absolute/path/to/atrinik-client/atrinik \
  --server="SERVER_HOST 17300 SERVER_FINGERPRINT" --stun_server=off --nometa
```

First exercise rejection using a separately recorded tuple with one fingerprint
hex digit changed. Require the explicit `QUIC certificate fingerprint mismatch` result and no
successful login; a timeout alone is not rejection proof.
Then select the verified tuple, log in interactively and record the actual
selected Vulkan device, gameplay, and audible sound separately. An image/font
load, PCM decode, software renderer or silent audio sink cannot replace those
hardware results. Do not send local scenario passwords into durable logs or
handoff text.

For persistence, create a player or make an observable gameplay change, stop the
client, then stop the exact server through the wrapper:

```sh
./atrinik down linux-headless
./atrinik ps linux-headless --json
./atrinik up --name linux-headless --profile classic \
  --state linux-review --service server --port 17300 --server-listener all-ipv4
./atrinik ps linux-headless --json
```

Reconnect using the fresh endpoint result and verify the saved player/change.
After final client shutdown, run `down` again and verify no live topology before
stopping only the exact owned container. Do not signal saved PIDs, delete lease
or control files, or use cleanup as shutdown. Keep the persistent state and
verification evidence. Windows/WSLg, MXE and native Windows D3D12 results remain
separate records.

## Portable binary evidence

`linux_export.portable_metadata_report` checks the six published portable metadata
records against hashes and immutable OCI coordinates supplied by the caller's
separate authenticated producer/registry verification. It accepts only the
Linux/amd64 Debian 12/glibc 2.36 portable target, checks the installed contract,
package and CPU declarations, resolves reported library/plugin edges, checks
versioned-symbol provider records and compares source/shader coordinates.
Duplicate, malformed, missing, disconnected or contradictory records fail.
Distinct SONAME declarations may share a dynamic feature name, as SDL does for
X11/Vulkan. Provider paths are aggregated by feature and can repeat when
distinct declarations resolve to the same object; repeated identical
declarations fail. Exclusions match exact object, feature and SONAME-list
declarations; they do not exclude other declarations sharing that feature.
The metadata lacks a SONAME-to-resolved-path map, so
`dynamic_soname_resolution_verified` remains false; reported paths do not prove
that every requested SONAME resolves.

The requested Classic commit must equal both the contract's consumer commit and
the shader source commit. The published consumer enforces this full-commit guard;
identical client/shader files at a different repository commit do not satisfy it.
A replacement producer contract requires its own review and publication.

This adapter only returns `metadata_consistent`. It does not fetch an image,
execute its tools, inspect its filesystem paths, verify registry provenance,
prove a source checkout/lease, rehash runtime binaries/source archives, validate
actual symbol definitions or establish legal/runtime/hardware qualification.
Caller-provided hashes are not self-authenticating. Keep those independent proofs
with the immutable image handoff and bind actual copied payload bytes during
later export integration. The image remains a build input, never coordinator
execution authority. Its X11/Vulkan client supports a Wayland desktop only via a
separately qualified XWayland setup; no native Wayland backend is claimed here.

`linux_export.inspect_elf` examines a leased, already-open regular ELF descriptor
with bounded, time-limited GNU readelf output; it never runs the payload or ldd.
It records class, endianness, machine, interpreter, static dependencies, runtime
search paths and version-needed records by provider. The structural dependency
report requires explicit bundled providers, materialized SONAME filenames and
host libraries, and rejects incompatible objects or escaping search paths.
The producer must configure the reported loader library directories explicitly.

Version definitions are parsed with unique names/indexes and complete auxiliary
parent/count records. Each bundled provider must define every version name its
resolved consumers require; a same-SONAME replacement with missing definitions
is rejected. BASE object names are not provided symbol versions. The report
records checked requirements and leaves host-library requirements explicitly
unverified, because their actual providers have not been inspected.
`provider_version_names_verified` describes only this version-name comparison.
It does not prove that individual symbols exist with the required versions;
`symbol_versions_verified` remains false. No version comparison establishes
source authority or replaces qualification against the actual host loader.

This report does not prove source provenance, symbol-version compatibility,
`dlopen` plugin closure, legal notices, or actual loader/hardware operation.
The source owner must bind it to the same copied bytes under its active lease.
A manifest supplied by a caller is evidence only. In particular, the pinned
Ubuntu 26.04 image uses glibc 2.43; moving its output does not demonstrate that it
runs on older supported native build hosts. Record required loader/provider
versions and qualify the exported application on each supported runtime baseline.

See the [GNU readelf reference](https://sourceware.org/binutils/docs/binutils/readelf.html)
for the inspected header, program-header, dynamic and version sections.


The independent `copy_payload` primitive accepts already-open source and exclusive
staging descriptors, copies bounded chunks, verifies the actual written digest,
and detects source/staging changes. It rejects Git LFS pointers and occupied
staging files. Only the authenticated source owner can provide the expected
record while holding its source/build lease; that owner must revalidate the
source inventory and staged destination before atomic publication. The primitive
never publishes or grants provenance. Failures leave the caller-owned staging
file for that owner's recovery workflow.

The launcher generator uses a relative `bin/atrinik` and
`share/games/atrinik` layout, supplies the export's `lib` directory, and starts
from the data directory supported by Classic's current-directory lookup.
`ATRINIK_CONFIG_DIR` selects separate persistent state; otherwise the launcher
uses `XDG_STATE_HOME/atrinik-client` or `HOME/.local/state/atrinik-client`.
State inside the payload is rejected. It requires the standard Linux shell and
coreutils, including `realpath`. Launcher fixture tests prove relocation and
argument/state handling only; they do not replace real client or media acceptance.
