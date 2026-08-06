# Contributing

Use Conventional Commits syntax for commits and pull-request titles:

~~~text
type(optional-scope)!: concise description
~~~

Examples include `fix(cli): reject mismatched checkout paths` and
`feat!: revise the profile schema`. Pull requests are squash-merged, and the
squash title becomes the release-driving commit on `master`.

Keep component implementation in its owning repository. Changes here should
remain focused on the manifest, multi-repository workflows, or their tests and
documentation. Preserve dirty component checkouts and persistent state during
manual validation.

Before opening a pull request, run:

~~~sh
python3 -m unittest discover -v
python3 -m compileall -q atrinik atrinik_workspace tests
./atrinik manifest validate
git diff --check
~~~

When changing the repository-local skill, also run the skill validator
available in the active Codex installation; its exact path is
environment-specific.

Exercise the smallest relevant real profile build as well. Changes to source
view, collection, runtime, or CMake composition should validate both client and
server with `--test`.
