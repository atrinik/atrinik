# Historical MIT provenance grants

This file is the exhaustive Atrinik registry for approved historical MIT
provenance grants. A registry entry is not an automatic license change for a
repository, file, or current-blame region.

| Grantor | Covered material | Permitted operations | Destination license |
| --- | --- | --- | --- |
| Zoey Rose | All original past Atrinik contributions solely authored by Zoey Rose | Copy, migrate, translate, or relicense | MIT |
| Daniel Liptrot | All original past Atrinik contributions solely authored by Daniel Liptrot | Copy, migrate, translate, or relicense | MIT |

These are affirmative permissions for the exact material they cover. The
checked-in `atrinik/classic` distribution and notices remain
GPL-2.0-or-later, but that repository license is not a blanket prohibition on
MIT reuse. Qualifying, independently separable material may be inspected and
used as implementation reference, copied, migrated or ported, translated or
adapted, and relicensed for an MIT destination. A source-informed
implementation derived only from qualifying material is provenance-approved
reuse; clean-room isolation is not required. Only the exact proven selected
material receives additional MIT permission in the recorded destination; no
surrounding file, dependency, asset, subtree, binary, or repository is
relicensed.

“Past” is temporal: each row covers only contributions completed before that
row was recorded. Zoey Rose's row was first recorded in
`d2af1c9fba462e5c18782d3c8206dbc5cfd74bb0`; Daniel Liptrot's row was first
recorded in `d64a8e958ca2adad783ad8912493d468a805f3fd`. A row is not a
prospective grant for later human- or agent-assisted work. Later material needs
its own contemporaneous compatible license or grant.

Before applying a grant:

1. Follow renames and moves through complete, non-shallow Git history for the
   exact source repository, path, and revision or revision range.
2. Prove that each selected contribution is the applicable named grantor's
   original work, that the grantor solely authored it, and that it falls within
   the row's temporal scope. Verify historical author identities; current blame
   alone is not proof.
3. Review every relevant change and the material itself for copied, generated,
   vendored, embedded third-party, or conflicting-licensed work. Fail closed on
   any history gap, unresolved identity, uncovered authorship, or uncertain
   origin.
4. Reuse only independently separable material wholly covered by the proof.
   Multiple rows may appear in one destination record only for distinct
   contributions that each independently satisfy one row's temporal and
   sole-original-authorship scope. Rows cannot be combined to cover a jointly
   authored contribution, generated output, or inseparable mixed work. Do not
   copy a mixed-authorship file merely because some surviving lines qualify.
5. Record the source repository/path/revision, destination repository/path,
   complete history and identity evidence, transformation, third-party review,
   applicable grantor and grant, and required notices in the destination pull
   request or a committed provenance manifest. Cite the exact
   `atrinik/atrinik` revision containing this registry as the grant evidence.

Use of an agent or other tool neither proves nor defeats permission to reuse
the exact material. A listed person's prompting, direction, selection, receipt
of output, or Git identity does not by itself place agent-generated output
within a row covering past contributions solely authored by that person. The
record must establish independently documented rights sufficient for the
intended destination for the exact output and every human or third-party input,
plus any applicable contemporaneous license or grant.

Third-party or separately licensed portions are not relicensed by these
historical grants. They may be included only under their own compatible terms
and required notices. If a destination must be solely MIT, exclude them unless
separate permission expressly authorizes MIT relicensing. Likewise, admitting
exact material for source reference, copying, porting, translation, adaptation,
or relicensing does not approve linking to, depending on, or bundling the GPL
Classic repository, library, or binary; dependency, package, and distribution
obligations remain separately governed.

Tests, fixtures, generated bindings, assets, and dependency code receive no
presumption of coverage. Each requires the same exact rights review plus the
destination repository's manifests, notices, and supply-chain treatment.

Independent implementation from documented observable behavior remains an
available path and is the default when qualifying reuse cannot be proven.
