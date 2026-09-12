# Filesystem paths and legacy identity metadata

The wrapper identifies managed filesystem resources by their normalized,
canonical paths. Moving the workspace storage to another SSD, rebuilding a
container, or mounting the same workspace through a new device does not
require a record migration or rewrite when every managed resource retains the
same canonical path.

New records do not write filesystem device, inode, or ctime fields. Readers
accept those fields from older records as inert compatibility metadata: the
values are ignored and never authorize, reject, or rebind a resource. The
former `migrate filesystem` command and remount-confirmation workflow no longer
exist.

Path-based coordination retains the wrapper's other safety checks. Operations
normalize and validate the expected managed path, reject symlinked or
unexpected object types, enforce ownership and mode rules, use ordinary file
locks and no-follow opens, and recheck generation tokens, content digests, Git
coordinates, process ownership, leases, and lifecycle state where applicable.
Hard-link and link-count checks remain where a transaction protocol uses them;
they do not establish resource identity.

Legacy records therefore continue to work after storage migration without an
operator recovery step. A missing path, unexpected symlink or type, invalid
owner or mode, changed content digest or generation, Git drift, active process,
or lease conflict still fails according to the owning workflow. Do not edit
managed JSON by hand; use the normal inspect, retry, release, or cleanup command
for that resource.
