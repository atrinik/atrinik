import { env } from "cloudflare:workers";
import {
  createExecutionContext,
  createScheduledController,
  waitOnExecutionContext,
} from "cloudflare:test";
import { beforeEach, describe, expect, it } from "vitest";

import worker from "../src/index";
import { deriveStoredKey, deriveUpdateProof } from "../src/protocol";

const BASE_URL = "https://meta.example.test";
const SOURCE_IP = "192.0.2.10";
const SERVER_ID = "1".repeat(64);
const CERTIFICATE = SERVER_ID;
const RAW_KEY = "a".repeat(128);
const COTP = "b".repeat(128);

async function callWorker(request: Request): Promise<Response> {
  const ctx = createExecutionContext();
  const response = await worker.fetch(request, env);
  await waitOnExecutionContext(ctx);
  return response;
}

function nextWebSocketMessage(socket: WebSocket): Promise<string> {
  return new Promise((resolve, reject) => {
    socket.addEventListener("message", (event) => resolve(String(event.data)), {
      once: true,
    });
    socket.addEventListener("error", () => reject(new Error("WebSocket error")), {
      once: true,
    });
  });
}

function request(path: string, init: RequestInit = {}, sourceIp = SOURCE_IP): Request {
  const headers = new Headers(init.headers);
  headers.set("X-Atrinik-Test-Source-IP", sourceIp);
  return new Request(`${BASE_URL}${path}`, { ...init, headers });
}

async function issueOtp(sourceIp = SOURCE_IP): Promise<string> {
  const response = await callWorker(request("/index.wsgi/otp", {}, sourceIp));
  expect(response.status).toBe(200);
  const body = await response.text();
  expect(body).toMatch(/^\{"otp": "[0-9a-f]{64}"\}$/);
  return (JSON.parse(body) as { otp: string }).otp;
}

function updateForm(
  otp: string,
  key: string,
  serverId = SERVER_ID,
): FormData {
  const form = new FormData();
  form.set("server_id", serverId);
  form.set("quic_host", "198.51.100.20");
  form.set("quic_port", "1730");
  form.set("quic_cert_sha256", serverId);
  form.set("num_players", "2");
  form.set("name", "Test Server");
  form.set("version", "4.0.0");
  form.set("text_comment", "Worker integration test");
  form.set("otp", otp);
  form.set("cotp", COTP);
  form.set("key", key);
  form.set("registration", "1");
  form.set("public", "1");
  return form;
}

async function postUpdate(form: FormData, sourceIp = SOURCE_IP): Promise<Response> {
  return callWorker(request("/index.wsgi/update", { method: "POST", body: form }, sourceIp));
}

beforeEach(async () => {
  await env.DB.batch([
    env.DB.prepare("DELETE FROM servers"),
    env.DB.prepare("DELETE FROM server_owners"),
    env.DB.prepare("DELETE FROM one_time_tokens"),
    env.DB.prepare("DELETE FROM rate_limits"),
    env.DB.prepare("DELETE FROM server_blacklist"),
  ]);
});

describe("metaserver Worker", () => {
  it("serves health and an empty QUIC directory", async () => {
    const health = await callWorker(request("/"));
    expect(health.status).toBe(200);
    expect(await health.json()).toEqual({ service: "Atrinik metaserver", status: "ok" });

    const listing = await callWorker(request("/v2/servers"));
    expect(listing.headers.get("Content-Type")).toBe("application/xml; charset=utf-8");
    expect(await listing.text()).toBe(
      '<?xml version="1.0" encoding="UTF-8"?><Servers protocol="3"></Servers>',
    );
    expect((await callWorker(request("/index.wsgi"))).status).toBe(404);
  });

  it("registers and accepts a second authenticated identity update", async () => {
    const firstOtp = await issueOtp();
    expect((await postUpdate(updateForm(firstOtp, RAW_KEY))).status).toBe(200);

    const storedKey = await deriveStoredKey(RAW_KEY, SERVER_ID);
    const owner = await env.DB.prepare(
      "SELECT server_id, auth_key FROM server_owners WHERE server_id = ?",
    )
      .bind(SERVER_ID)
      .first<{ server_id: string; auth_key: string }>();
    expect(owner).toEqual({ server_id: SERVER_ID, auth_key: storedKey });

    const secondOtp = await issueOtp();
    const proof = await deriveUpdateProof(secondOtp, storedKey, COTP);
    const secondForm = updateForm(secondOtp, proof);
    secondForm.set("registration", "0");
    secondForm.set("quic_host", "198.51.100.21");
    secondForm.set("quic_port", "1731");
    expect((await postUpdate(secondForm)).status).toBe(200);

    const body = await (await callWorker(request("/v2/servers"))).text();
    expect(body).toContain(`<Id>${SERVER_ID}</Id>`);
    expect(body).toContain("<Address>198.51.100.21</Address><Port>1731</Port>");
    expect(body).not.toContain("198.51.100.20");
  });

  it("rejects replayed, expired, wrong-IP, and incorrect-key tokens", async () => {
    const otp = await issueOtp();
    expect((await postUpdate(updateForm(otp, RAW_KEY))).status).toBe(200);
    expect((await postUpdate(updateForm(otp, RAW_KEY))).status).toBe(401);

    const expired = await issueOtp();
    await env.DB.prepare("UPDATE one_time_tokens SET expires_at = 0").run();
    expect((await postUpdate(updateForm(expired, RAW_KEY))).status).toBe(401);

    const wrongIp = await issueOtp();
    expect((await postUpdate(updateForm(wrongIp, RAW_KEY), "192.0.2.11")).status).toBe(401);

    const storedKey = await deriveStoredKey(RAW_KEY, SERVER_ID);
    const wrongKeyOtp = await issueOtp();
    const wrongProof = await deriveUpdateProof(wrongKeyOtp, storedKey, "c".repeat(128));
    expect((await postUpdate(updateForm(wrongKeyOtp, wrongProof))).status).toBe(401);
  });

  it("fails closed when an existing update has no ownership record", async () => {
    const missingId = "3".repeat(64);
    const otp = await issueOtp();
    const form = updateForm(otp, "d".repeat(128), missingId);
    form.set("registration", "0");
    expect((await postUpdate(form)).status).toBe(409);
    expect(await env.DB.prepare(
      "SELECT server_id FROM server_owners WHERE server_id = ?",
    ).bind(missingId).first()).toBeNull();
  });

  it("allows only one concurrent first claim for an identity", async () => {
    const [firstOtp, secondOtp] = await Promise.all([issueOtp(), issueOtp()]);
    const responses = await Promise.all([
      postUpdate(updateForm(firstOtp, "a".repeat(128))),
      postUpdate(updateForm(secondOtp, "c".repeat(128))),
    ]);
    expect(responses.map((response) => response.status).sort()).toEqual([200, 401]);
  });

  it("rejects an explicitly oversized update body", async () => {
    const otp = await issueOtp();
    const response = await callWorker(request("/index.wsgi/update", {
      method: "POST",
      body: updateForm(otp, RAW_KEY),
      headers: { "Content-Length": "100001" },
    }));
    expect(response.status).toBe(413);

    const streamed = await callWorker(request("/index.wsgi/update", {
      method: "POST",
      body: "x".repeat(100_001),
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
    }));
    expect(streamed.status).toBe(413);
  });

  it("enforces identity and source-IP blacklist patterns", async () => {
    await env.DB.prepare(
      "INSERT INTO server_blacklist (pattern, reason, created_at) VALUES (?, ?, ?)",
    ).bind("1111*", "test identity block", 1).run();
    expect((await postUpdate(updateForm(await issueOtp(), RAW_KEY))).status).toBe(403);

    await env.DB.prepare("DELETE FROM server_blacklist").run();
    await env.DB.prepare(
      "INSERT INTO server_blacklist (pattern, reason, created_at) VALUES (?, ?, ?)",
    ).bind("192.0.2.*", "test address block", 1).run();
    expect((await postUpdate(updateForm(await issueOtp(), RAW_KEY))).status).toBe(403);
  });

  it("removes stale listings and expired operational rows on schedule", async () => {
    await env.DB.prepare(
      `INSERT INTO server_owners
         (server_id, auth_key, current_ip, ip_changed_at, created_at, updated_at)
       VALUES (?, ?, ?, 0, 0, 0)`,
    ).bind(SERVER_ID, "a".repeat(128), SOURCE_IP).run();
    await env.DB.prepare(
      `INSERT INTO servers
         (server_id, source_ip, name, players_count, version, text_comment,
          last_seen, is_public, quic_host, quic_port, quic_cert_sha256,
          password_required, rendezvous_token_hash)
       VALUES (?, ?, 'Stale', 0, '4.0.0', 'stale', 0, 1, ?, 1730, ?, 0, ?)`,
    ).bind(SERVER_ID, SOURCE_IP, SOURCE_IP, CERTIFICATE, "f".repeat(64)).run();
    await env.DB.prepare(
      "INSERT INTO one_time_tokens (token_hash, source_ip, expires_at, created_at) VALUES ('old', ?, 0, 0)",
    ).bind(SOURCE_IP).run();
    await env.DB.prepare(
      "INSERT INTO rate_limits (source_ip, scope, window_start, request_count) VALUES (?, 'old', 0, 1)",
    ).bind(SOURCE_IP).run();

    await worker.scheduled(createScheduledController(), env, createExecutionContext());

    for (const table of ["servers", "one_time_tokens", "rate_limits"]) {
      const row = await env.DB.prepare(`SELECT COUNT(*) AS count FROM ${table}`)
        .first<{ count: number }>();
      expect(row?.count).toBe(0);
    }
    const owners = await env.DB.prepare("SELECT COUNT(*) AS count FROM server_owners")
      .first<{ count: number }>();
    expect(owners?.count).toBe(1);
  });

  it("lists opted-in servers and relays rendezvous candidates", async () => {
    const response = await postUpdate(updateForm(await issueOtp(), RAW_KEY));
    expect(response.status).toBe(200);
    const result = await response.json<{ rendezvousToken: string; status: string }>();
    expect(result.status).toBe("ok");
    expect(result.rendezvousToken).toMatch(/^[0-9a-f]{64}$/);

    const body = await (await callWorker(request("/v2/servers"))).text();
    expect(body).toContain(`<Id>${SERVER_ID}</Id>`);
    expect(body).toContain("<Address>198.51.100.20</Address>");
    expect(body).not.toContain("<Hostname>");

    const upgradeRequired = await callWorker(
      request(`/v2/rendezvous/${SERVER_ID}?role=client`),
    );
    expect(upgradeRequired.status).toBe(426);

    const queryToken = await callWorker(request(
      `/v2/rendezvous/${SERVER_ID}?role=server&token=${result.rendezvousToken}`,
      { headers: { Upgrade: "websocket" } },
    ));
    expect(queryToken.status).toBe(401);
    expect(queryToken.headers.get("WWW-Authenticate")).toBe("Bearer");

    const authorized = await callWorker(request(
      `/v2/rendezvous/${SERVER_ID}?role=server`,
      {
        headers: {
          Authorization: `Bearer ${result.rendezvousToken}`,
          Upgrade: "websocket",
        },
      },
    ));
    expect(authorized.status).toBe(101);
    const serverSocket = authorized.webSocket!;
    serverSocket.accept();

    const clientRendezvous = await callWorker(request(
      `/v2/rendezvous/${SERVER_ID}?role=client`,
      { headers: { Upgrade: "websocket" } },
    ));
    expect(clientRendezvous.status).toBe(101);
    const clientSocket = clientRendezvous.webSocket!;
    clientSocket.accept();

    const ticket = "c".repeat(64);
    const offered = nextWebSocketMessage(serverSocket);
    clientSocket.send(JSON.stringify({
      type: "client_candidate",
      host: "::ffff:192.0.2.44",
      port: 49_152,
      ticket,
    }));
    expect(JSON.parse(await offered)).toEqual({
      type: "client_candidate",
      host: "0000:0000:0000:0000:0000:ffff:c000:022c",
      port: 49_152,
      ticket,
    });

    const candidate = nextWebSocketMessage(clientSocket);
    serverSocket.send(JSON.stringify({
      type: "server_candidate",
      host: "2001:db8::20",
      port: 1_730,
      kind: "ipv6",
      ticket,
    }));
    expect(JSON.parse(await candidate)).toEqual({
      type: "server_candidate",
      host: "2001:0db8:0000:0000:0000:0000:0000:0020",
      port: 1_730,
      kind: "ipv6",
      ticket,
    });

    const completed = nextWebSocketMessage(clientSocket);
    serverSocket.send(JSON.stringify({ type: "complete", ticket }));
    expect(JSON.parse(await completed)).toEqual({ type: "complete", ticket });

    clientSocket.close(1000, "Test complete");
    serverSocket.close(1000, "Test complete");
  });
});
