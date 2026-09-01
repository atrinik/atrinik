# Filesystem identity and remount recovery

The wrapper uses two deliberately different identity classes.

* Durable records use the versioned portable identity object. It contains the
  object kind, inode, and file mode; regular files also retain ctime and may
  retain a content digest. It never stores `st_dev`, because that number is
  assigned by the current mount namespace.
* Live fencing uses an identity obtained from an opened descriptor. The
  complete `(st_dev, st_ino)` pair, and the mount identifier where relevant,
  remains an ephemeral race and replacement check. It is not a durable
  rebind authority.

This distinction preserves replacement, symlink, hard-link, and TOCTOU
protections while allowing a remounted workspace to retain its durable
records. A legacy `{ "device": ..., "inode": ... }` record remains readable,
but a changed device is never guessed to be a remount by an ordinary command.
Rename-prone lease records use the same portable kind/inode/mode projection
without ctime; their opened-descriptor, generation, and content fences remain
ephemeral and continue to protect the live lifecycle.

## Recovery procedure

Run the commands from the wrapper checkout that owns the records:

```sh
./atrinik migrate filesystem --dry-run --json
./atrinik migrate filesystem --apply --confirm-remount
./atrinik migrate filesystem --audit --json
```

The dry run inventories legacy records without changing them. It includes the
physical lease anchor, workspace JSON records (including topology and state
records), and managed delivery-ledger JSON records and transaction sidecars under
`build/reviews/`. Apply requires
the explicit remount confirmation, proves the same inode and safe target for
each record, and writes a durable transaction journal before publishing any
replacement. Each pre-image is kept in the journal, along with portable
pre/post identities and legacy evidence. An interrupted transaction resumes
only when every record is still exactly its before or after snapshot; otherwise
it fails closed. A failed transaction rolls back the records it touched and
retains a rollback identity for the next exact retry. Audit verifies both the
snapshot digest and the portable identity, so replacing a record with
byte-identical content does not silently rebind it.

`ATRINIK_WORKSPACE_DIR` is honored for workspace records. The physical lease
anchor and review sidecars are discovered from the owning wrapper repository.
Do not edit the JSON or journal by hand, and do not use a path-only or
inode-only workaround for a missing target. An ambiguous, changed, symlinked,
foreign-owned, or otherwise unproven target must be repaired or restored and
then re-audited.

New writers use portable identities. Readers accept the legacy pair only at
the compatibility boundary, and require this explicit migration before a
changed mount device can be accepted. The migration is separate from cleanup,
scope release, repository migration, and topology shutdown; none of those
commands implicitly rebinds a legacy record.
