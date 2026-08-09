# Historical MIT provenance grants

This file is the exhaustive Atrinik registry for approved historical MIT
provenance grants. A registry entry is not an automatic license change for a
repository, file, or current-blame region.

| Grantor | Covered material | Permitted operations | Destination license |
| --- | --- | --- | --- |
| Zoey Rose | All original past Atrinik contributions solely authored by Zoey Rose | Copy, migrate, translate, or relicense | MIT |
| Daniel Liptrot | All original past Atrinik contributions solely authored by Daniel Liptrot | Copy, migrate, translate, or relicense | MIT |

Before applying a grant:

1. Follow renames and moves through complete, non-shallow Git history for the
   exact source repository, path, and revision or revision range.
2. Prove that the selected material is the named grantor's original work and
   that the grantor solely authored it. Verify historical author identities;
   current blame alone is not proof.
3. Review every relevant change and the material itself for copied, generated,
   vendored, embedded third-party, or conflicting-licensed work. Fail closed on
   any history gap, unresolved identity, mixed authorship, or uncertain origin.
4. Reuse only independently separable material covered by the proof. Do not
   copy a mixed-authorship file merely because some surviving lines qualify.
5. Record the source repository/path/revision, destination repository/path,
   complete history and identity evidence, transformation, third-party review,
   applicable grantor and grant, and required notices in the destination pull
   request or a committed provenance manifest. Cite the exact
   `atrinik/atrinik` revision containing this registry as the grant evidence.

Independent implementation from documented behavior remains the default when
reuse cannot be proven.
