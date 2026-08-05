PRAGMA foreign_keys = ON;

CREATE TABLE server_owners (
    hostname TEXT PRIMARY KEY,
    auth_key TEXT NOT NULL,
    current_ip TEXT NOT NULL,
    ip_changed_at INTEGER NOT NULL,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL
);

CREATE TABLE servers (
    hostname TEXT NOT NULL,
    port INTEGER NOT NULL,
    source_ip TEXT NOT NULL,
    port_crypto INTEGER,
    name TEXT NOT NULL,
    players_count INTEGER NOT NULL,
    version TEXT NOT NULL,
    text_comment TEXT NOT NULL,
    cert_pubkey TEXT,
    server_cert TEXT,
    server_cert_sig TEXT,
    last_seen INTEGER NOT NULL,
    PRIMARY KEY (hostname, port),
    FOREIGN KEY (hostname) REFERENCES server_owners(hostname)
);

CREATE INDEX servers_last_seen_idx ON servers(last_seen);

CREATE TABLE one_time_tokens (
    token_hash TEXT PRIMARY KEY,
    source_ip TEXT NOT NULL,
    expires_at INTEGER NOT NULL,
    created_at INTEGER NOT NULL
);

CREATE INDEX one_time_tokens_expires_idx ON one_time_tokens(expires_at);

CREATE TABLE rate_limits (
    source_ip TEXT NOT NULL,
    scope TEXT NOT NULL,
    window_start INTEGER NOT NULL,
    request_count INTEGER NOT NULL,
    PRIMARY KEY (source_ip, scope)
);

CREATE TABLE server_blacklist (
    pattern TEXT PRIMARY KEY,
    reason TEXT NOT NULL DEFAULT '',
    created_at INTEGER NOT NULL
);
