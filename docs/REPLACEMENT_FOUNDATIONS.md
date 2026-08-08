# Replacement foundation evidence

The machine-readable source of truth is
[`governance/replacement-foundations.json`](../governance/replacement-foundations.json).
It maps every fresh replacement repository to its M1 issues, merged proof pull
requests, stable aggregate check, contribution/provenance guidance, dependency
policy, notices, package/SBOM contract, and mixed-license boundary.

The repositories do not inherit permission to reuse classic code. New MIT work
is the default. A historical grant is usable only through the exhaustive root
registry at the exact revision recorded in the inventory and only after a
complete, non-shallow, rename-aware history audit proves the named grantor's
sole original authorship. The review must also verify identity, embedded
third-party material, exact source/destination paths, transformation,
copyright, grant, and reviewer. Mixed or uncertain candidates are excluded;
current blame, committer identity, or a repository-level ownership inference is
never enough.

## Reproducible decisions

The independently reproducible admitted example is
`atrinik/content-toolkit:provenance/reuse.json#lossless-core-model`. It records
six separable source blobs, their complete one-touch history, verified GitHub
identity, embedded-material review, transformation into one Rust destination,
attribution, and the pinned root grant. From a complete content checkout, run:

```sh
tools/audit-provenance.sh --source /absolute/non-shallow/content/checkout
```

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
4. Separate only material whose sole original authorship and embedded licenses
   are proven. Exclude everything else.
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

