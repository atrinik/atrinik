# Metaserver Worker deployment

Deployment changes public discovery and authentication state. These steps are
an operator runbook, not authorization for an automated tool to deploy.

## Prepare

1. Confirm the target Cloudflare account, zone, Worker name, D1 database, and
   custom domain with a second operator.
2. Back up the target D1 database and record a recovery bookmark.
3. Replace the placeholder D1 ID through deployment-specific configuration;
   do not commit production identifiers or credentials.
4. Run `npm ci` and `npm run check` from this directory.
5. Review `wrangler deploy --dry-run --outdir dist` output for unexpected
   bindings, routes, compatibility changes, or secrets.

Migration `0005_quic_only.sql` is intentionally breaking: it only supports QUIC
identities and directory protocol 3, and removes prior listings and owners. Do
not apply it unless the database has been backed up and the reset was explicitly
approved.

## Canary

1. Create a separate canary D1 database and apply
   all ordered migrations to it.
2. Deploy a canary Worker with no production custom domain.
3. Verify `/`, OTP issuance, authenticated registration/update,
   `/v2/servers`, and both rendezvous WebSocket roles.
4. Confirm malformed identities, expired/replayed OTPs, missing bearer tokens,
   oversized bodies, and blacklist entries fail closed.
5. Confirm logs contain no authentication material or rendezvous tokens.

## Cut over

1. Pause server updates or accept a short directory repopulation window.
2. Apply the clean schema to the approved production D1 database.
3. Deploy the reviewed artifact with the production D1 and Durable Object
   bindings.
4. Attach `meta.atrinik.org` as a Custom Domain only after the canary passes.
5. Register controlled servers and verify their certificate identity,
   endpoint, visibility, and rendezvous flow from a current client.
6. Monitor rejection rates, D1 errors, Durable Object failures, response time,
   and stale-record cleanup through at least one scheduled run.

## Administrative SQL

`scripts/admin_sql.py` never connects to Cloudflare. It emits SQL for operator
review:

```sh
python3 scripts/admin_sql.py reset-owner SERVER_ID
python3 scripts/admin_sql.py blacklist-add '1111*' 'reason'
python3 scripts/admin_sql.py blacklist-remove '1111*'
```

An owner reset deletes both ownership and listing rows for exactly one
64-character server identity. The server must also reset its local metaserver
authentication key before re-registering. Treat this as destructive recovery.

## Roll back

Detach the production Custom Domain or deploy the last known-good artifact,
then restore the recorded D1 backup if schema or ownership state was changed.
Do not attempt to roll back to the removed TCP/hostname contract. Preserve logs
and the failed artifact for diagnosis before retrying.
