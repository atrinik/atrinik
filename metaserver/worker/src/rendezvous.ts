import { DurableObject } from "cloudflare:workers";

import {
  constantTimeEqual,
  normalizeIpAddress,
  SERVER_SIGNAL_CANDIDATE_KINDS,
  sha256Hex,
} from "./protocol";
import type { DirectCandidateKind } from "./protocol";
import type { ServerRecord } from "./types";

const MAX_SIGNAL_BYTES = 512;
const HEX_64 = /^[0-9a-f]{64}$/;

export async function openRendezvous(
  request: Request,
  env: Env,
  serverId: string,
): Promise<Response> {
  if (!HEX_64.test(serverId)) {
    return new Response("Invalid server ID\n", { status: 400 });
  }
  if (request.headers.get("Upgrade")?.toLowerCase() !== "websocket") {
    return new Response("WebSocket upgrade required\n", { status: 426 });
  }

  const url = new URL(request.url);
  const role = url.searchParams.get("role");
  const cutoff = Math.floor(Date.now() / 1_000) -
    Number(env.LISTING_TTL_SECONDS ?? "3600");
  const server = await env.DB.prepare(
    `SELECT server_id, is_public, rendezvous_token_hash, last_seen
       FROM servers
      WHERE server_id = ? AND last_seen >= ?
      ORDER BY last_seen DESC LIMIT 1`,
  ).bind(serverId, cutoff).first<ServerRecord>();

  if (server === null) {
    return new Response("Server is offline\n", { status: 404 });
  }

  if (role === "server") {
    const authorization = request.headers.get("Authorization") ?? "";
    const match = /^Bearer ([0-9a-f]{64})$/i.exec(authorization);
    const token = match?.[1] ?? "";
    const actual = await sha256Hex(token);
    if (server.rendezvous_token_hash === null ||
        server.rendezvous_token_hash === undefined ||
        !constantTimeEqual(actual, server.rendezvous_token_hash)) {
      return new Response("Invalid rendezvous token\n", {
        status: 401,
        headers: { "WWW-Authenticate": "Bearer" },
      });
    }
  } else if (role === "client") {
    if (server.is_public !== 1) {
      return new Response("Server is private\n", { status: 403 });
    }
  } else {
    return new Response("Invalid rendezvous role\n", { status: 400 });
  }

  const id = env.RENDEZVOUS.idFromName(serverId);
  const headers = new Headers(request.headers);
  headers.delete("Authorization");
  headers.set("X-Atrinik-Role", role);
  return env.RENDEZVOUS.get(id).fetch(new Request(request, { headers }));
}

/**
 * Signaling-only rendezvous room. It accepts no HTTP bodies and no binary
 * WebSocket messages, and forwards only validated direct-candidate signaling.
 */
export class RendezvousRoom extends DurableObject<Env> {
  constructor(
    ctx: DurableObjectState,
    env: Env,
  ) {
    super(ctx, env);
  }

  async fetch(request: Request): Promise<Response> {
    const role = request.headers.get("X-Atrinik-Role");
    if (role !== "server" && role !== "client") {
      return new Response("Forbidden\n", { status: 403 });
    }

    if (role === "client" &&
        this.ctx.getWebSockets("client").length >= 64) {
      return new Response("Rendezvous room is full\n", { status: 503 });
    }
    if (role === "server") {
      for (const existing of this.ctx.getWebSockets("server")) {
        existing.close(1000, "Server control connection replaced");
      }
    }

    const pair = new WebSocketPair();
    const client = pair[0];
    const room = pair[1];
    this.ctx.acceptWebSocket(room, [role]);
    return new Response(null, { status: 101, webSocket: client });
  }

  async webSocketMessage(socket: WebSocket, message: string | ArrayBuffer) {
    if (typeof message !== "string" ||
        new TextEncoder().encode(message).byteLength > MAX_SIGNAL_BYTES) {
      socket.close(1008, "Invalid signaling message");
      return;
    }

    let signal: {
      type?: unknown;
      host?: unknown;
      port?: unknown;
      kind?: unknown;
      ticket?: unknown;
    };
    try {
      signal = JSON.parse(message);
    } catch {
      socket.close(1008, "Invalid signaling JSON");
      return;
    }

    let candidateHost: string | null = null;
    if (typeof signal.host === "string") {
      try {
        candidateHost = normalizeIpAddress(signal.host);
      } catch {
        candidateHost = null;
      }
    }
    const isClientCandidate = signal.type === "client_candidate" &&
      candidateHost !== null &&
      Number.isInteger(signal.port) &&
      Number(signal.port) >= 1 && Number(signal.port) <= 65_535 &&
      typeof signal.ticket === "string" &&
      /^[0-9a-f]{64}$/.test(signal.ticket);
    const isServerCandidate = signal.type === "server_candidate" &&
      candidateHost !== null &&
      Number.isInteger(signal.port) &&
      Number(signal.port) >= 1 && Number(signal.port) <= 65_535 &&
      typeof signal.kind === "string" &&
      SERVER_SIGNAL_CANDIDATE_KINDS.has(signal.kind as DirectCandidateKind) &&
      typeof signal.ticket === "string" &&
      /^[0-9a-f]{64}$/.test(signal.ticket);
    const isComplete = signal.type === "complete" &&
      typeof signal.ticket === "string" &&
      /^[0-9a-f]{64}$/.test(signal.ticket);
    if (!isClientCandidate && !isServerCandidate && !isComplete) {
      socket.close(1008, "Unsupported signaling message");
      return;
    }

    const sourceIsServer = this.ctx.getTags(socket).includes("server");
    if ((sourceIsServer && !isServerCandidate && !isComplete) ||
        (!sourceIsServer && !isClientCandidate)) {
      socket.close(1008, "Message not allowed for role");
      return;
    }

    const forwarded = isClientCandidate
      ? JSON.stringify({
          type: "client_candidate",
          host: candidateHost,
          port: signal.port,
          ticket: signal.ticket,
        })
      : isServerCandidate
        ? JSON.stringify({
            type: "server_candidate",
            host: candidateHost,
            port: signal.port,
            kind: signal.kind,
            ticket: signal.ticket,
          })
        : JSON.stringify({ type: "complete", ticket: signal.ticket });
    const targets = this.ctx.getWebSockets(
      sourceIsServer ? "client" : "server",
    );
    for (const target of targets) {
      try {
        target.send(forwarded);
      } catch {
        // Hibernating sockets can disappear between enumeration and send.
      }
    }
  }

  async webSocketClose(
    socket: WebSocket,
    code: number,
    reason: string,
  ) {
    socket.close(code, reason);
  }
}
