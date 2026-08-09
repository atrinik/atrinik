# Contributing

Use Conventional Commits syntax for commits and pull-request titles:

~~~text
type(optional-scope)!: concise description
~~~

Examples include `fix(cli): reject mismatched checkout paths` and
`feat!: revise the profile schema`. Pull requests are squash-merged, and the
squash title becomes the release-driving commit on `main`.

Keep component implementation in its owning repository. Changes here should
remain focused on the manifest, multi-repository workflows, or their tests and
documentation. Preserve dirty physical checkouts and persistent state during
manual validation.

The canonical `server`, `client`, `editor`, `protocol`, `renderer`,
`content-toolkit`, and `website` repositories form the MIT replacement stack.
Plain `./atrinik init` is replacement/default-only. Exact
`./atrinik init --with classic` adds the complete currently playable classic
cohort: the `atrinik/classic` monorepo checkout, `content-1x@1.x`, and retained
GPL tools. The monorepo supplies the logical classic client, server, editor,
protocol, and libatrinik components from source subdirectories. Do not put
those checkouts in the default cohort or mix replacement and classic providers
in one runnable profile. Replacement repositories have validated standalone M1
foundations; their wrapper build/runtime adapters and integrated service
closure have not landed, so current game integration uses a profile created
from `classic`.

For a pre-split workspace, initialize only the destination with
`./atrinik init classic`, run `./atrinik migrate repositories --dry-run`
before apply, and finish with `--audit`. The checked migration combines proven
pre-monorepo repositories under `classic/`, preserves recoverable
originals and worktree state, refuses ambiguous or unsafe layouts, and rewrites
proven classic profiles atomically. States, builds, runtimes, and logs remain
outside the repository-layout migration. It uses integrated commit-map targets
when available and imports an exact verified local commit when a branch-only
target disappeared with the retired classic `history/*` namespace.

Before opening a pull request, run:

~~~sh
python3 -m pip install --requirement requirements-dev.txt
python3 -m coverage run -m unittest discover -v
python3 -m coverage report --show-missing
python3 -m compileall -q atrinik atrinik_workspace tests
python3 -m atrinik_workspace.guidance_inventory --check
./atrinik manifest validate
./atrinik supply-chain validate
git diff --check
~~~

When changing the repository-local skill, also run the skill validator
available in the active Codex installation; its exact path is
environment-specific.

Exercise the smallest relevant real profile build as well. Changes to current
source-view, collection, runtime, or CMake composition should validate both
classic client and server with `--profile classic --test`. Replacement
components use their repository-owned aggregate validation today and remain
inspectable through wrapper manifest/profile contracts until wrapper build
adapters are implemented.
