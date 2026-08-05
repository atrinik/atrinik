ALTER TABLE servers ADD COLUMN server_id TEXT;
ALTER TABLE servers ADD COLUMN is_public INTEGER NOT NULL DEFAULT 1;
ALTER TABLE servers ADD COLUMN connectivity_mode TEXT NOT NULL DEFAULT 'legacy_tcp';
ALTER TABLE servers ADD COLUMN quic_host TEXT;
ALTER TABLE servers ADD COLUMN quic_port INTEGER;
ALTER TABLE servers ADD COLUMN quic_cert_sha256 TEXT;
ALTER TABLE servers ADD COLUMN password_required INTEGER NOT NULL DEFAULT 0;

CREATE UNIQUE INDEX servers_server_id_idx
    ON servers(server_id)
    WHERE server_id IS NOT NULL;

CREATE INDEX servers_public_direct_idx
    ON servers(is_public, connectivity_mode, last_seen);
