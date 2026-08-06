# Contributing

Make component changes in the repository that owns the affected source, then
update `components.lock.json` only after that component publishes an immutable
release. Integration pull requests must use a Conventional Commits title and
pass `python3 -m unittest discover -s tests -p 'test_*.py'` plus
`scripts/build.sh`.

Do not vendor component source, add Git submodules, or weaken release-asset
digest checks. Preserve each component's license and attribution; the MIT
license in this repository applies only to its integration scripts and
documentation.
