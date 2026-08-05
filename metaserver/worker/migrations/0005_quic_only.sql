-- Intentional breaking reset: remove hostname/TCP discovery and ownership.
DROP TABLE servers;
DROP TABLE server_owners;

CREATE TABLE server_owners (
    server_id TEXT PRIMARY KEY CHECK (
        length(server_id) = 64 AND server_id NOT GLOB '*[^0-9a-f]*'
    ),
    auth_key TEXT NOT NULL CHECK (
        length(auth_key) = 128 AND auth_key NOT GLOB '*[^0-9a-f]*'
    ),
    current_ip TEXT NOT NULL,
    ip_changed_at INTEGER NOT NULL,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL
);

CREATE TABLE servers (
    server_id TEXT PRIMARY KEY REFERENCES server_owners(server_id),
    source_ip TEXT NOT NULL,
    name TEXT NOT NULL,
    players_count INTEGER NOT NULL CHECK (players_count >= 0),
    version TEXT NOT NULL,
    text_comment TEXT NOT NULL,
    last_seen INTEGER NOT NULL,
    is_public INTEGER NOT NULL CHECK (is_public IN (0, 1)),
    quic_host TEXT NOT NULL,
    quic_port INTEGER NOT NULL CHECK (quic_port BETWEEN 1 AND 65535),
    quic_cert_sha256 TEXT NOT NULL CHECK (
        length(quic_cert_sha256) = 64 AND
        quic_cert_sha256 NOT GLOB '*[^0-9a-f]*' AND
        quic_cert_sha256 = server_id
    ),
    password_required INTEGER NOT NULL CHECK (password_required IN (0, 1)),
    rendezvous_token_hash TEXT NOT NULL CHECK (
        length(rendezvous_token_hash) = 64 AND
        rendezvous_token_hash NOT GLOB '*[^0-9a-f]*'
    )
);

CREATE INDEX servers_last_seen_idx ON servers(last_seen);
CREATE INDEX servers_public_idx ON servers(is_public, last_seen);
