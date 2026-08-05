import {
  constantTimeEqual,
  deriveStoredKey,
  deriveUpdateProof,
  envBoolean,
  escapeXml,
  envNumber,
  formatOtpResponse,
  normalizeIpAddress,
  parseUpdatePayload,
  randomToken,
  RequestError,
  sha256Hex,
} from "./protocol";
import type {
  BlacklistRecord,
  OwnerRecord,
  ServerRecord,
  UpdatePayload,
} from "./types";
import { openRendezvous, RendezvousRoom } from "./rendezvous";
export { RendezvousRoom };

const OTP_PATH = "/index.wsgi/otp";
const UPDATE_PATH = "/index.wsgi/update";
const V2_LIST_PATH = "/v2/servers";
const V2_RENDEZVOUS_PREFIX = "/v2/rendezvous/";

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    try {
      const url = new URL(request.url);
      const sourceIp = sourceAddress(request, env);
      await enforceNativeRateLimit(env.GLOBAL_RATE_LIMITER, sourceIp);
      if (url.pathname === "/") {
        return Response.json({ service: "Atrinik metaserver", status: "ok" });
      }
      if (url.pathname === V2_LIST_PATH && request.method === "GET") {
        return await listDirectServers(env);
      }
      if (url.pathname.startsWith(V2_RENDEZVOUS_PREFIX) &&
          request.method === "GET") {
        await enforceNativeRateLimit(env.RENDEZVOUS_RATE_LIMITER, sourceIp);
        return await openRendezvous(request, env,
          url.pathname.slice(V2_RENDEZVOUS_PREFIX.length));
      }
      if (url.pathname === OTP_PATH && request.method === "GET") {
        return await issueOtp(request, env);
      }
      if (url.pathname === UPDATE_PATH && request.method === "POST") {
        return await updateServer(request, env);
      }
      return new Response("Not found\n", { status: 404 });
    } catch (error) {
      if (error instanceof RequestError) {
        console.warn(JSON.stringify({
          event: "request_rejected",
          method: request.method,
          path: new URL(request.url).pathname,
          status: error.status,
          reason: error.message,
        }));
        return new Response(`${error.message}\n`, { status: error.status });
      }
      console.error(error);
      return new Response("Internal server error\n", { status: 500 });
    }
  },

  async scheduled(
    _controller: ScheduledController,
    env: Env,
    _ctx: ExecutionContext,
  ): Promise<void> {
    const now = Math.floor(Date.now() / 1_000);
    const staleCutoff =
      now - envNumber(env.STALE_DATA_RETENTION_SECONDS, 2_592_000);
    await env.DB.batch([
      env.DB.prepare("DELETE FROM servers WHERE last_seen < ?").bind(staleCutoff),
      env.DB.prepare("DELETE FROM one_time_tokens WHERE expires_at < ?").bind(now),
      env.DB.prepare("DELETE FROM rate_limits WHERE window_start < ?").bind(now - 86_400),
    ]);
  },
} satisfies ExportedHandler<Env>;

async function listDirectServers(env: Env): Promise<Response> {
  const cutoff =
    Math.floor(Date.now() / 1_000) -
    envNumber(env.LISTING_TTL_SECONDS, 3_600);
  const result = await env.DB.prepare(
    `SELECT server_id, name, players_count, version, text_comment,
            quic_host, quic_port, quic_cert_sha256,
            password_required, last_seen
       FROM servers
      WHERE last_seen >= ?
        AND is_public = 1
        AND server_id IS NOT NULL
      ORDER BY name COLLATE NOCASE, server_id`,
  )
    .bind(cutoff)
    .all<ServerRecord>();

  const body = result.results.map((server) =>
    "<Server>" +
    `<Id>${escapeXml(server.server_id ?? "")}</Id>` +
    `<Name>${escapeXml(server.name)}</Name>` +
    `<PlayersCount>${server.players_count}</PlayersCount>` +
    `<Version>${escapeXml(server.version)}</Version>` +
    `<TextComment>${escapeXml(server.text_comment || "No description.")}</TextComment>` +
    `<Address>${escapeXml(server.quic_host ?? "")}</Address>` +
    `<Port>${server.quic_port}</Port>` +
    `<CertificateSha256>${server.quic_cert_sha256}</CertificateSha256>` +
    `<PasswordRequired>${server.password_required === 1 ? "true" : "false"}</PasswordRequired>` +
    "</Server>"
  ).join("");

  return new Response(
    `<?xml version="1.0" encoding="UTF-8"?><Servers protocol="3">${body}</Servers>`,
    {
      headers: {
        "Cache-Control": "public, max-age=5",
        "Content-Type": "application/xml; charset=utf-8",
      },
    },
  );
}

async function issueOtp(request: Request, env: Env): Promise<Response> {
  const sourceIp = sourceAddress(request, env);
  const now = Math.floor(Date.now() / 1_000);
  await enforceNativeRateLimit(env.OTP_RATE_LIMITER, sourceIp);
  await enforceRateLimit(env, sourceIp, "otp", now);

  const token = randomToken();
  const tokenHash = await sha256Hex(token);
  const expiresAt = now + envNumber(env.OTP_TTL_SECONDS, 120);
  await env.DB.prepare(
    "INSERT INTO one_time_tokens (token_hash, source_ip, expires_at, created_at) VALUES (?, ?, ?, ?)",
  )
    .bind(tokenHash, sourceIp, expiresAt, now)
    .run();

  return new Response(formatOtpResponse(token), {
    headers: {
      "Cache-Control": "no-store",
      "Content-Type": "application/json; charset=utf-8",
    },
  });
}

async function updateServer(request: Request, env: Env): Promise<Response> {
  const sourceIp = sourceAddress(request, env);
  const now = Math.floor(Date.now() / 1_000);
  await enforceNativeRateLimit(env.UPDATE_RATE_LIMITER, sourceIp);
  await enforceRateLimit(env, sourceIp, "update", now);

  const payload = parseUpdatePayload(await readUpdateForm(request, env));
  await enforceBlacklist(env, payload.serverId, sourceIp);
  await consumeOtp(env, payload.otp, sourceIp, now);

  let owner = await env.DB.prepare(
    "SELECT server_id, auth_key, current_ip, ip_changed_at, updated_at FROM server_owners WHERE server_id = ?",
  )
    .bind(payload.serverId)
    .first<OwnerRecord>();

  if (owner !== null) {
    await authenticateExistingOwner(payload, owner);
  } else {
    if (!payload.registration) {
      throw new RequestError(
        "The server identity has no ownership record; explicit registration is required",
        409,
      );
    }

    const storedKey = await deriveStoredKey(payload.key, payload.serverId);
    const insertion = await env.DB.prepare(
      `INSERT OR IGNORE INTO server_owners
         (server_id, auth_key, current_ip, ip_changed_at, created_at, updated_at)
       VALUES (?, ?, ?, ?, ?, ?)`,
    )
      .bind(payload.serverId, storedKey, sourceIp, now, now, now)
      .run();

    owner = await env.DB.prepare(
      "SELECT server_id, auth_key, current_ip, ip_changed_at, updated_at FROM server_owners WHERE server_id = ?",
    )
      .bind(payload.serverId)
      .first<OwnerRecord>();

    if (owner === null) {
      throw new Error("Owner registration was not persisted");
    }
    if (!insertion.meta.changes && !constantTimeEqual(storedKey, owner.auth_key)) {
      throw new RequestError("Invalid metaserver key", 401);
    }
  }

  const rendezvousToken = randomToken();
  const rendezvousTokenHash = await sha256Hex(rendezvousToken);
  await env.DB.batch([
    env.DB.prepare(
      `UPDATE server_owners
          SET current_ip = ?,
              ip_changed_at = CASE WHEN current_ip <> ? THEN ? ELSE ip_changed_at END,
              updated_at = ?
        WHERE server_id = ?`,
    ).bind(sourceIp, sourceIp, now, now, payload.serverId),
    env.DB.prepare(
      `INSERT INTO servers
         (server_id, source_ip, name, players_count, version, text_comment,
          last_seen, is_public, quic_host, quic_port, quic_cert_sha256,
          password_required, rendezvous_token_hash)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
       ON CONFLICT(server_id) DO UPDATE SET
         source_ip = excluded.source_ip,
         name = excluded.name,
         players_count = excluded.players_count,
         version = excluded.version,
         text_comment = excluded.text_comment,
         last_seen = excluded.last_seen,
         is_public = excluded.is_public,
         quic_host = excluded.quic_host,
         quic_port = excluded.quic_port,
         quic_cert_sha256 = excluded.quic_cert_sha256,
         password_required = excluded.password_required,
         rendezvous_token_hash = excluded.rendezvous_token_hash`,
    ).bind(
      payload.serverId,
      sourceIp,
      payload.name,
      payload.playersCount,
      payload.version,
      payload.textComment,
      now,
      payload.isPublic ? 1 : 0,
      payload.quicHost ?? sourceIp,
      payload.quicPort,
      payload.quicCertSha256,
      payload.passwordRequired ? 1 : 0,
      rendezvousTokenHash,
    ),
  ]);

  return Response.json(
    { status: "ok", rendezvousToken },
    { headers: { "Cache-Control": "no-store" } },
  );
}

async function readUpdateForm(request: Request, env: Env): Promise<FormData> {
  const maximum = envNumber(env.MAX_UPDATE_BODY_BYTES, 100_000);
  const rawLength = request.headers.get("Content-Length");
  if (rawLength !== null && (!/^\d+$/.test(rawLength) || Number(rawLength) > maximum)) {
    throw new RequestError("Update request is too large", 413);
  }

  if (request.body === null) {
    throw new RequestError("Missing update body");
  }
  const reader = request.body.getReader();
  const chunks: Uint8Array[] = [];
  let size = 0;
  for (;;) {
    const { done, value } = await reader.read();
    if (done) {
      break;
    }
    size += value.byteLength;
    if (size > maximum) {
      await reader.cancel();
      throw new RequestError("Update request is too large", 413);
    }
    chunks.push(value);
  }

  const body = new Uint8Array(size);
  let offset = 0;
  for (const chunk of chunks) {
    body.set(chunk, offset);
    offset += chunk.byteLength;
  }
  return new Request(request.url, {
    method: "POST",
    headers: request.headers,
    body,
  }).formData();
}

async function enforceBlacklist(
  env: Env,
  serverId: string,
  sourceIp: string,
): Promise<void> {
  const match = await env.DB.prepare(
    `SELECT pattern, reason
       FROM server_blacklist
      WHERE ? GLOB pattern OR ? GLOB pattern
      LIMIT 1`,
  )
    .bind(serverId, sourceIp)
    .first<BlacklistRecord>();

  if (match !== null) {
    console.warn(JSON.stringify({
      event: "blacklist_match",
      serverId,
      sourceIp,
      pattern: match.pattern,
      reason: match.reason,
    }));
    throw new RequestError("The server is blacklisted", 403);
  }
}

async function authenticateExistingOwner(payload: UpdatePayload, owner: OwnerRecord): Promise<void> {
  const expected = await deriveUpdateProof(payload.otp, owner.auth_key, payload.cotp);
  if (!constantTimeEqual(expected, payload.key)) {
    throw new RequestError("Invalid metaserver key", 401);
  }
}

async function consumeOtp(env: Env, token: string, sourceIp: string, now: number): Promise<void> {
  const tokenHash = await sha256Hex(token);
  const consumed = await env.DB.prepare(
    `DELETE FROM one_time_tokens
      WHERE token_hash = ? AND source_ip = ? AND expires_at >= ?
      RETURNING token_hash`,
  )
    .bind(tokenHash, sourceIp, now)
    .first<{ token_hash: string }>();
  if (consumed === null) {
    throw new RequestError("Invalid or expired one-time token", 401);
  }
}

async function enforceRateLimit(
  env: Env,
  sourceIp: string,
  scope: string,
  now: number,
): Promise<void> {
  const windowStart = Math.floor(now / 60) * 60;
  const row = await env.DB.prepare(
    `INSERT INTO rate_limits (source_ip, scope, window_start, request_count)
     VALUES (?, ?, ?, 1)
     ON CONFLICT(source_ip, scope) DO UPDATE SET
       window_start = excluded.window_start,
       request_count = CASE
         WHEN rate_limits.window_start = excluded.window_start
         THEN rate_limits.request_count + 1
         ELSE 1
       END
     RETURNING request_count`,
  )
    .bind(sourceIp, scope, windowStart)
    .first<{ request_count: number }>();

  if (row === null || row.request_count > envNumber(env.RATE_LIMIT_PER_MINUTE, 30)) {
    throw new RequestError("Too many requests", 429);
  }
}

async function enforceNativeRateLimit(
  limiter: RateLimit,
  key: string,
): Promise<void> {
  const result = await limiter.limit({ key });
  if (!result.success) {
    throw new RequestError("Too many requests", 429);
  }
}

function sourceAddress(request: Request, env: Env): string {
  const testOverride = envBoolean(env.ALLOW_TEST_SOURCE_IP, false)
    ? request.headers.get("X-Atrinik-Test-Source-IP")
    : null;
  const address = testOverride ?? request.headers.get("CF-Connecting-IP");
  if (address === null) {
    throw new RequestError("The source address is unavailable", 400);
  }
  try {
    return normalizeIpAddress(address);
  } catch {
    throw new RequestError("The source address is invalid", 400);
  }
}
