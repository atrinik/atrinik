# Atrinik metaserver Worker

This Cloudflare Worker is the directory and rendezvous service for Atrinik's
QUIC-only game transport. It does not proxy game traffic.

## Endpoints

- `GET /` returns service health.
- `GET /index.wsgi/otp` issues a source-bound, single-use update token.
- `POST /index.wsgi/update` authenticates a server identity and updates its
  QUIC directory record.
- `GET /v2/servers` returns protocol 3 XML with public QUIC endpoints and
  pinned SHA-256 certificate fingerprints.
- `GET /v2/rendezvous/:server-id` upgrades to a signaling-only WebSocket.

There is deliberately no TCP directory, compatibility listing, DNS ownership,
or game-port reachability probe. A server is owned by the SHA-256 identity
derived from its persistent QUIC certificate, and the directory rejects a
record whose identity and certificate fingerprint differ. Update authentication
retains the native server's OTP/proof protocol. Rendezvous server peers additionally
authenticate with the bearer token returned by a successful update.

## Development

Use Node.js 20 or newer:

```sh
npm ci
npm run check
```

`npm run check` runs TypeScript checks, the local Workers runtime tests, the
Python SQL-generator tests, and a Wrangler dry-run. Generated output belongs in
`dist/` and must not be edited.

The checked-in Wrangler file has a placeholder D1 ID and no production route.
Supply reviewed production bindings during the deployment procedure. Never run
remote migrations, deployments, or owner resets merely to validate a change.

## Storage

`migrations/0005_quic_only.sql` is the intentional breaking reset from the old
hostname/TCP schema. It retains the ordered migration history but removes old
listings and owners because their hostname-derived credentials cannot safely
claim certificate identities. Ownership resets and blacklist changes should be
generated with `scripts/admin_sql.py`, reviewed, and only then applied by an
authorized operator.

See [DEPLOYMENT.md](DEPLOYMENT.md) for the release checklist.
