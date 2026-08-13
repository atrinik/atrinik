# Replacement foundation evidence

The machine-readable source of truth is
[`governance/replacement-foundations.json`](../governance/replacement-foundations.json).
It maps every fresh replacement repository to its M1 issues, merged proof pull
requests, stable aggregate check, contribution/provenance guidance, dependency
policy, notices, package/SBOM contract, and mixed-license boundary.

The repositories do not automatically inherit permission to reuse arbitrary
Classic code. [`PROVENANCE.md`](PROVENANCE.md) is the current exhaustive grant
registry and expressly permits qualifying material to be used as source
reference, copied, migrated or ported, translated or adapted, and relicensed
for an MIT destination. That provenance-approved path is not clean-room work.
Independent implementation is the default only when exact grant coverage
cannot be proven. Qualifying means that an independently separable contribution
falls within one row's temporal and sole-original-authorship scope. Later work
requires contemporaneous compatible permission; multiple rows cannot be
combined to cover joint authorship, generated output, or an inseparable work.

Existing reproducible decisions retain the exact historical registry path and
revision they used; new decisions cite the merged wrapper revision containing
this file. A grant remains usable only after a complete, non-shallow,
rename-aware history audit proves the applicable grantor's sole original
authorship and temporal scope. The review must also verify identity,
separability, embedded third-party material, exact source/destination paths,
transformation, copyright, grant, and reviewer. Current blame, committer
identity, agent direction, or a repository-level ownership inference is never
enough. The Classic source repository remains under its existing license;
grant-qualified source reuse does not approve GPL dependencies or bundles.

## Reproducible decisions

The independently reproducible admitted example is
`atrinik/content-toolkit:provenance/reuse.json#lossless-core-model`. It records
six separable source blobs, their complete one-touch history, verified GitHub
identity, embedded-material review, transformation into one Rust destination,
attribution, and the pinned root grant. From a complete content checkout, run:

```sh
tools/audit-provenance.sh --source /absolute/non-shallow/content/checkout
```

This historical decision remains governed by its pinned registry revision and
evidence contract. It is not a substitute for the current review requirements
when recording a new reuse decision.

The excluded example is
`atrinik/content-toolkit:provenance/reuse.json#remaining-content-tools`. The
same audit proves the wider tree has mixed authorship, GPL license text, and
authored fixtures/data. `tools/check-provenance.sh` verifies that all 50 paths
are assigned exactly once and that the excluded set has no destination.

## Review process

For future reuse:

1. Start with the owning repository's `CONTRIBUTING.md` and `PROVENANCE.md`.
2. Fetch complete source history and prove it is not shallow.
3. Follow every candidate path through renames and moves; record immutable
   revisions and blob IDs, every author/committer identity, and independent
   GitHub identity evidence.
4. Separate only material whose original authorship, temporal scope, and exact
   reuse permission are proven. Separately licensed portions retain their own
   terms and notices and cannot be described as MIT-relicensed unless their
   permission expressly allows it. Exclude everything else.
5. Record every field listed in `required_record_fields`, pin the exact root
   grant-registry revision, and add a reproducible checker before importing.
6. Keep dependency allowlists, lockfiles, third-party notices, fixture/asset
   allowlists, package manifests, and SBOM generation consistent with the
   repository's actual language and artifact boundary.
7. Have a reviewer reproduce the evidence from a clean non-shallow checkout.

An engine package may stay MIT while carrying or referring to content, sound,
resources, downloads, or media under different licenses. Each repository's
`license_boundary` states that separation; generated notices and SBOMs report
the real component licenses instead of flattening a distribution to MIT.
