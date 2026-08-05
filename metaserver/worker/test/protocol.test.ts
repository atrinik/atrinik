import { describe, expect, it } from "vitest";

import {
  constantTimeEqual,
  deriveStoredKey,
  deriveUpdateProof,
  envBoolean,
  envNumber,
  escapeXml,
  formatOtpResponse,
  ipAddressesEqual,
  normalizeIpAddress,
  parseUpdatePayload,
  RequestError,
  sha512Hex,
} from "../src/protocol";

function validForm(): FormData {
  const form = new FormData();
  form.set("server_id", "1".repeat(64));
  form.set("quic_host", "198.51.100.20");
  form.set("quic_port", "1730");
  form.set("quic_cert_sha256", "1".repeat(64));
  form.set("num_players", "2");
  form.set("name", "Test & Friends");
  form.set("version", "4.0.0");
  form.set("text_comment", "Ready <now>");
  form.set("otp", "otp-value");
  form.set("cotp", "b".repeat(128));
  form.set("key", "a".repeat(128));
  form.set("registration", "1");
  return form;
}

describe("protocol helpers", () => {
  it("matches the C authentication formula", async () => {
    expect(await sha512Hex("abc")).toBe(
      "ddaf35a193617abacc417349ae204131" +
      "12e6fa4e89a97ea20a9eeee64b55d39a" +
      "2192992a274fc1a836ba3c23a3feebbd" +
      "454d4423643ce80e2a9ac94fa54ca49f",
    );
    const stored = await deriveStoredKey("a".repeat(128), "1".repeat(64));
    const proof = await deriveUpdateProof("otp", stored, "b".repeat(128));
    expect(proof).toMatch(/^[0-9a-f]{128}$/);
    expect(constantTimeEqual(proof, proof)).toBe(true);
    expect(constantTimeEqual(proof, "0".repeat(128))).toBe(false);
    expect(constantTimeEqual(proof, "short")).toBe(false);
  });

  it("keeps the exact OTP response expected by the C parser", () => {
    expect(formatOtpResponse("token")).toBe('{"otp": "token"}');
  });

  it("canonicalizes IP addresses", () => {
    expect(normalizeIpAddress("[2001:0DB8::1]")).toBe(
      "2001:0db8:0000:0000:0000:0000:0000:0001",
    );
    expect(ipAddressesEqual("::ffff:192.0.2.1", "0:0:0:0:0:ffff:c000:201")).toBe(true);
  });

  it("uses secure fallbacks for malformed environment settings", () => {
    expect(envBoolean("treu", true)).toBe(true);
    expect(envBoolean("off", true)).toBe(false);
    expect(envNumber("30junk", 30)).toBe(30);
    expect(envNumber("60", 30)).toBe(60);
  });

  it("parses a complete QUIC update and rejects malformed fields", () => {
    const parsed = parseUpdatePayload(validForm());
    expect(parsed.serverId).toBe("1".repeat(64));
    expect(parsed.quicPort).toBe(1730);
    expect(parsed.playersCount).toBe(2);
    expect(parsed.isPublic).toBe(false);

    for (const [field, value] of [
      ["quic_port", "1730junk"],
      ["quic_port", "0"],
      ["quic_port", "65536"],
      ["num_players", "2players"],
      ["num_players", "4294967296"],
      ["server_id", "z".repeat(64)],
      ["quic_cert_sha256", "2".repeat(63)],
      ["quic_cert_sha256", "2".repeat(64)],
    ]) {
      const form = validForm();
      form.set(field, value);
      expect(() => parseUpdatePayload(form)).toThrow(RequestError);
    }
  });

  it("escapes XML control and markup characters", () => {
    expect(escapeXml("<&a\u0000b'\"")).toBe("&lt;&amp;ab&apos;&quot;");
  });
});
