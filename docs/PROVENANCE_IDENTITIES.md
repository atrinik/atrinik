# Privacy-preserving provenance identities

This is the operating policy for the canonical public provenance identity
registry in `governance/provenance-identities/registry.json`. The registry and
its versioned schema belong only to `atrinik/atrinik`; component repositories
store exact material provenance and an immutable reference, never a copied
alias registry. All examples and current records are synthetic.

## Threat model and classification

The protected subject may have used a former name, pseudonym, email address,
account, signing key, or other identifier. An observer can search Git history,
issues, pull requests, Actions artifacts and logs, compare timestamps and
commit ranges, brute-force unkeyed hashes, and correlate fields across
repositories. Removing an `alias` field alone does not prevent
re-identification.

Information is classified as follows:

- A **public alias** is a display name and alias set that the affected person
  explicitly authorized for publication. The restricted authorization records
  the exact fields and date. Prior public appearance is not authorization.
- **Restricted identity evidence** includes mappings, contact data, raw
  statements, account history, exact private review notes, salts and keys. It
  never enters public Git, GitHub discussion, CI input/output, artifacts, or
  local review reports.
- A **public confidential attestation** is the minimum opaque record proving
  that an authorized reviewer checked identity, sole authorship, temporal
  scope, and the rights grant. It contains no subject identifier or exact
  material coordinate.
- The **exact material scope** remains in the destination repository's
  provenance record. An opaque `scope_binding` joins it to the attestation
  without identifying the subject.
- **Integrity metadata** includes schema/policy versions, reviewer, dates,
  status, keyed restricted-evidence integrity, canonical public-record digest,
  and immutable registry/schema digests. It is public only after the complete
  record passes correlation review.

Public record review considers the whole diff and surrounding metadata,
including identifier shape, reviewer and date combinations, source/destination
coordinates, commit ranges, grant names, ordering, and changes near other
work. If those facts make the subject readily inferable, the record must be
coarsened, delayed, split, or rejected. Confidential reconciliation fails
closed whenever safe publication cannot be demonstrated.

## Restricted evidence decision

Restricted evidence is stored as encrypted, versioned objects in a dedicated
private Atrinik organization repository named `provenance-evidence`, separate
from every public source repository. Each object is encrypted before upload to
the current `provenance-custodians` age recipient set. The repository contains
only ciphertext, an opaque record ID, a keyed HMAC over the canonical decrypted
record, and encrypted correction/supersession history. Plaintext, encryption
identities, HMAC keys, recovery material, and local exports are excluded from
Git and GitHub.

The repository must remain unprovisioned, or confidential decisions must fail
closed, until all of these controls exist:

1. The organization owners designate at least two custodians in the
   `provenance-custodians` team and at least two rights reviewers in the
   `provenance-reviewers` team. A person may submit evidence but may not be the
   sole reviewer of their own identity or grant.
2. Custodians keep decryption and HMAC material in distinct organization-owned
   password-manager vault items with individual access, mandatory MFA, no
   shared plaintext secret, and an offline recovery copy held by a second
   custodian.
3. Branch protection requires two custodian reviews for ciphertext changes.
   GitHub organization audit-log entries and a restricted access ledger record
   read/export purpose, requester, approver, object IDs, and disposition.
4. A reviewer receives only the minimum decrypted evidence through an
   ephemeral encrypted workspace, records claim results separately, and
   destroys exports after review. CI and GitHub Actions never decrypt evidence.

Evidence is retained while any active or archived Atrinik revision relies on
it, plus seven years after the last reliance is withdrawn. A subject may ask a
custodian to correct or withdraw evidence. Corrections create a new restricted
object and superseding public record; history is not rewritten. Withdrawal or
unresolvable dispute revokes the public record and blocks new reuse. Archived
or deleted repositories retain exact repository/revision/path facts in their
own surviving provenance record, not in the identity registry.

Suspected disclosure freezes new attestations, revokes affected public records
where reliance is unsafe, preserves restricted audit evidence, notifies
organization owners and affected people privately, and triggers scope review.
Key compromise rotates recipients and HMAC keys, re-encrypts every active
object, emits new keyed integrity values and superseding public records, and
revokes records that cannot be reverified. Public incident text must not reveal
the mapping it is protecting.

## Attestation and integrity contract

An approved reviewer attests only after restricted evidence proves all four
claims: historical identity, exact original/sole authorship, temporal inclusion
in the applicable grant, and rights sufficient for the destination operation.
The reviewer creates a random public `record_id`, restricted object ID, and
`scope_binding`; none is derived from personal data or source coordinates.
Unkeyed hashes of names, aliases, emails, commits, or ranges are forbidden.

`atrinik-json-v1` serializes UTF-8 JSON with lexicographically sorted object
keys, no insignificant whitespace, JSON string escaping, and no floating-point
values. A record's public SHA-256 covers every field except `integrity`. The
restricted HMAC uses a separately held key and covers the canonical decrypted
evidence plus the exact scope binding. Public aliases additionally require a
restricted authorization naming every public identity field. Every public
record and exact component-scope statement carries an Ed25519 SSH signature
from a key in the versioned `reviewers.json` roster. The validator checks the
key's identity, role boundary, effective dates and current revocation status;
plain SHA-256 integrity is never treated as authorization.

Reviews expire within 366 days and must be re-reviewed before use. `revoked`
records carry an effective date and privacy-safe reason and are invalid
immediately; `superseded` records carry an effective date and active replacement
pointer without mutating history. Schema or policy changes use a
new version and require explicit migration review. The validator rejects
unknown versions, duplicate IDs/bindings, stale active records, unsafe fields,
digest drift, and references to non-active records.

## Immutable component references

A component provenance record owns exact source repository, path and full Git
revision; destination repository and path; transformation; and the opaque scope
binding. Its `evidence_reference` has exactly:

- `repository: atrinik/atrinik`;
- a full 40-character coordinator commit reachable from canonical `origin/main`;
- the opaque record ID;
- SHA-256 digests of the registry and schema bytes; and
- the canonical GitHub `blob/<commit>/.../registry.json#<record-id>` permalink.

Online review opens that permalink and verifies that the commit is on the
coordinator default branch or covered by the cited release. Bounded offline
review uses an existing non-shallow canonical coordinator checkout containing
the pinned commit. It disables replace objects, rejects grafts, proves ancestry
from `origin/main`, sizes each object before reading at most 1 MiB, and reads
only the pinned registry, schema and reviewer roster. It does not fetch, search
history, or consult a local alias copy. A signed-release mode is not implemented
in schema v1 and therefore fails closed.

The synthetic component-owned records for `atrinik/client` and
`atrinik/server` exercise both paths. The copies in this repository are test
fixtures; each component keeps its own identical reference record and validates
it without copying the canonical registry, schema, or reviewer roster:

```sh
./atrinik provenance validate \
  --reference tests/fixtures/provenance-identities/positive/synthetic-alpha.json \
  --reference tests/fixtures/provenance-identities/positive/synthetic-beta.json
```

Their pinned URLs become online after this branch is pushed. Before merge they
may be exercised only with `--non-authorizing-audit-ref
origin/feat/privacy-preserving-provenance-registry`; that mode is visibly not an
approval for reuse. They carry an authenticated `synthetic: true` boundary and
are test evidence, not permission for real material. Production validation
accepts only a revision that has landed on the default branch.

## Migration and review

For each existing or future component provenance manifest:

1. Preserve its exact source/destination and transformation facts. Inventory
   every identity field or alias copy and stop if removing it would obscure an
   unresolved rights decision.
2. Obtain explicit publication authorization for public aliases, or create
   restricted evidence and an opaque confidential attestation through the
   two-role review above. Never infer authorization from old public metadata.
3. Run whole-record correlation review. Commit only the component reference and
   exact material scope, then validate online and with the bounded offline
   command. Do not copy canonical records or schema into the component.
4. On correction, add a superseding record and update the component reference
   in an ordinary reviewed change. On withdrawal, compromise, expiry, or
   revocation, fail closed and stop reuse until a new active record is approved.
5. For renamed, transferred, archived, or deleted repositories, retain the
   immutable original coordinate and archived commit evidence. A repository's
   disappearance never turns a movable URL, cached alias file, or unverifiable
   record into acceptable proof.

Current named grants in `docs/PROVENANCE.md` are not silently converted into
alias records. They remain public grant statements, but any historical identity
reconciliation must use this process and an active record. No real
confidential mapping may be added through a public issue or pull request.
