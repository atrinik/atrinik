ALTER TABLE servers ADD COLUMN rendezvous_token_hash TEXT;
CREATE INDEX servers_rendezvous_id_idx ON servers(server_id, last_seen);
