# Native Classic developer toolchain

Use Ubuntu 26.04 amd64 as the complete native developer setup baseline below.
The package and source versions reproduce the owner contract used by Classic
`4998131ad2ae4c9680685fd87e2d85de1dc15fd9` in its Ubuntu 26.04 build container.
These translated host commands have not yet been qualified on a direct host;
record that qualification separately under #593 after the authority change is
accepted. Container CI is evidence for its container environment.

Native authority eligibility on Ubuntu 24.04/26.04 and Debian 12/13 does not
establish development-library availability on every version. Debian 12 is the
portable client's glibc 2.36 ABI baseline; its pinned producer builds/tests the
client only. Debian native client/server compilation and other host versions
need their own complete dependency and test qualification. Use the pinned
coordinator/build environment when the host cannot supply the contract below.
An exported client needs no compiler, development headers or this setup.

## Install the complete Ubuntu 26.04 contract

The host owner performs package installation on the selected development host.
Use a fresh qualification host if the exact package lock conflicts with installed
versions; do not remove packages or force a downgrade to make it fit. These
commands keep persistent apt source files unchanged. They require an existing
working CA trust store and authenticated `gh` access to the source repository;
see [coordinator authentication](COORDINATOR_AUTH.md). They do not log in, change
credentials or install anything in a runtime/client container.

Run in Bash. Fetch immutable owner inputs into a new private directory and
verify every file before running its script:

```sh
set -euo pipefail
test "$(dpkg --print-architecture)" = amd64
test "$(. /etc/os-release; printf '%s' "$ID:$VERSION_ID")" = ubuntu:26.04
mkdir -p "$HOME/.cache"
inputs=$(mktemp -d "$HOME/.cache/atrinik-native-inputs.XXXXXXXX")
install -d -m 700 "$inputs/tools"
owner_revision=aa7e944ec3afcf2eaec434b164618680d514a77b
for name in classic-packages.lock audio-toolchain.json tools/build-sdl3-mixer.sh; do
  gh api "repos/atrinik/devcontainer/contents/$name?ref=$owner_revision" \
    --jq .content | base64 -d >"$inputs/$name"
done
printf '%s  %s\n' \
  3c11d4e2b6d11302ba0cb4689dab958a00468965a05e337e191ddace5bbb12d0 "$inputs/classic-packages.lock" \
  7155a2eb7498eebe99544dfe12a7bb38b5fa7d545335acb959b75181d685743f "$inputs/audio-toolchain.json" \
  f7c106bb3004c48e25b4a495ce26e36753129f2d657e1e3934c6b5ddfa87ffe1 "$inputs/tools/build-sdl3-mixer.sh" \
  | sha256sum -c -
chmod 700 "$inputs/tools/build-sdl3-mixer.sh"
```

The lock is one `PACKAGE=EXACT_DEB_VERSION` per line. It includes the C17
compiler, CMake/Ninja, Python development headers, Git/LFS, pkg-config, coverage
and Check/subunit tools, Flex, MiniUPnPc, curl, zlib, gd, idn2, XML2, readline,
SDL3 3.4.2, SDL3_image 3.4.0, SDL3_ttf 3.2.2 and OpenSSL 3.5.5. OpenSSL 3.5+
is necessary for the complete QUIC transport and test contract. SDL2 is not a
substitute. GPU/display packages in this development lock do not make the
headless server depend on a display or GPU.

Select the owner's signed Ubuntu snapshot through a private copy of the
host's standard Ubuntu source file. Keeping signed package verification while
disabling only snapshot expiry permits this fixed historical snapshot; retain
TLS certificate verification:

```sh
cp -- /etc/apt/sources.list.d/ubuntu.sources "$inputs/ubuntu-snapshot.sources"
sed -i \
  -e 's|^URIs:.*|URIs: https://snapshot.ubuntu.com/ubuntu/20260810T000000Z/|' \
  -e '/^Check-Valid-Until:/d' \
  "$inputs/ubuntu-snapshot.sources"
sed -i '/^Signed-By:/a Check-Valid-Until: no' "$inputs/ubuntu-snapshot.sources"
apt_snapshot_options=(
  -o "Dir::Etc::sourcelist=$inputs/ubuntu-snapshot.sources"
  -o 'Dir::Etc::sourceparts=-'
  -o 'Acquire::Retries=5'
)
sudo apt-get "${apt_snapshot_options[@]}" update
mapfile -t locked_packages <"$inputs/classic-packages.lock"
sudo apt-get "${apt_snapshot_options[@]}" install --no-remove --no-install-recommends \
  "${locked_packages[@]}"
while IFS='=' read -r package version; do
  test "$(dpkg-query --show --showformat='${Version}' "$package")" = "$version"
done <"$inputs/classic-packages.lock"
```

## Build the pinned audio libraries in a private prefix

The verified owner builder takes four positional arguments: audio manifest,
CMake command, installation prefix and job count. It downloads and hashes
SDL3_mixer 3.2.4, libogg 1.3.5, libopus 1.4 and libopusfile 0.12, builds a shared
mixer with the pinned static codecs, and removes its private temporary source
build on exit. It has no download-cache argument. The enabled decoder set is
WAV, built-in Vorbis, Opus and dr_mp3; this is the same application contract as
the pinned producer.

```sh
prefix=$(mktemp -d "$HOME/.cache/atrinik-native-prefix.XXXXXXXX")
"$inputs/tools/build-sdl3-mixer.sh" \
  "$inputs/audio-toolchain.json" cmake "$prefix" "$(nproc)"
export CMAKE_PREFIX_PATH="$prefix${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
export PKG_CONFIG_PATH="$prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export LD_LIBRARY_PATH="$prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
pkg-config --exact-version=3.4.2 sdl3
pkg-config --exact-version=3.4.0 sdl3-image
pkg-config --exact-version=3.2.2 sdl3-ttf
pkg-config --exact-version=3.2.4 sdl3-mixer
pkg-config --atleast-version=3.5 openssl
```

Keep this prefix and the three exported variables for subsequent builds and
runtime launches; record its exact path in the handoff. It is independent of
mutable player/configuration state. Do not replace system libraries or run
`ldconfig` for this prefix.

## Build, test and run through the wrapper

After the native authority implementation is accepted, complete the live probe
and normal issue/project preparation before delivery work. From the selected
wrapper checkout, install wrapper test dependencies in its private virtual
environment as described in [CONTRIBUTING](../CONTRIBUTING.md). Then:

```sh
./atrinik init --with classic
./atrinik profile show classic --json
./atrinik build all --profile classic --test
./atrinik up --name native-classic-check --profile classic --temporary-state
./atrinik ps native-classic-check --json
./atrinik logs native-classic-check server --tail 100
./atrinik down native-classic-check
```

Delivery workers additionally plan and bind these fresh resources through their
ledger before creation; the example names grant no right to reuse an existing
resource. `build all --test` covers the integrated Classic client/server graph.
The wrapper acquires and validates the pinned DXC/SPIRV-Cross shader toolchain
from the Classic owner locks, retaining the executable/library hashes and shader
identity. No guessed generated shader path or unversioned system shader compiler
is needed. Do not bypass a source/cohort compatibility refusal.

Retain distribution/version, compiler and library versions, source revisions,
profile and wrapper-reported build paths, complete test results and runtime
endpoint/fingerprint evidence. A successful build or software-rendered launch
does not prove hardware gameplay or audible playback. Follow the separate
server/client, wrong-pin and persistence procedures in
[Linux execution](LINUX_EXECUTION.md); native qualification remains separate from
portable export, WSLg and native Windows qualification.
