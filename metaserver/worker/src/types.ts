export interface BlacklistRecord {
  pattern: string;
  reason: string;
}

export interface ServerRecord {
  server_id: string;
  source_ip: string;
  name: string;
  players_count: number;
  version: string;
  text_comment: string;
  last_seen: number;
  is_public: number;
  quic_host: string;
  quic_port: number;
  quic_cert_sha256: string;
  password_required: number;
  rendezvous_token_hash: string | null;
}

export interface OwnerRecord {
  server_id: string;
  auth_key: string;
  current_ip: string;
  ip_changed_at: number;
  created_at: number;
  updated_at: number;
}

export interface UpdatePayload {
  serverId: string;
  name: string;
  playersCount: number;
  version: string;
  textComment: string;
  otp: string;
  cotp: string;
  key: string;
  registration: boolean;
  isPublic: boolean;
  quicHost: string | null;
  quicPort: number;
  quicCertSha256: string;
  passwordRequired: boolean;
}
