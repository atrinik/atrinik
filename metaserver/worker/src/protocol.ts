import type { UpdatePayload } from "./types";

const HEX_64 = /^[0-9a-f]{64}$/;
const HEX_128 = /^[0-9a-f]{128}$/;

export const DIRECT_CANDIDATE_KINDS = [
  "lan",
  "ipv6",
  "prflx",
  "mapped",
  "srflx",
  "directory",
] as const;
export type DirectCandidateKind = typeof DIRECT_CANDIDATE_KINDS[number];
export const SERVER_SIGNAL_CANDIDATE_KINDS = new Set<DirectCandidateKind>([
  "lan",
  "ipv6",
  "mapped",
  "srflx",
]);

export class RequestError extends Error {
  constructor(
    message: string,
    readonly status = 400,
  ) {
    super(message);
  }
}

export function envNumber(
  value: string | undefined,
  fallback: number,
  minimum = 1,
): number {
  if (value === undefined) {
    return fallback;
  }

  if (!/^\d+$/.test(value)) {
    return fallback;
  }

  const parsed = Number(value);
  return Number.isFinite(parsed) && parsed >= minimum ? parsed : fallback;
}

export function envBoolean(
  value: string | undefined,
  fallback: boolean,
): boolean {
  if (value === undefined) {
    return fallback;
  }

  const normalized = value.toLowerCase();
  if (["1", "true", "yes", "on"].includes(normalized)) {
    return true;
  }
  if (["0", "false", "no", "off"].includes(normalized)) {
    return false;
  }
  return fallback;
}

export async function sha512Hex(value: string): Promise<string> {
  const digest = await crypto.subtle.digest(
    "SHA-512",
    new TextEncoder().encode(value),
  );
  return bytesToHex(new Uint8Array(digest));
}

export async function sha256Hex(value: string): Promise<string> {
  const digest = await crypto.subtle.digest(
    "SHA-256",
    new TextEncoder().encode(value),
  );
  return bytesToHex(new Uint8Array(digest));
}

function bytesToHex(bytes: Uint8Array): string {
  return Array.from(bytes, (byte) => byte.toString(16).padStart(2, "0")).join("");
}

export async function deriveStoredKey(
  registrationKey: string,
  ownerId: string,
): Promise<string> {
  return sha512Hex(registrationKey + ownerId);
}

export async function deriveUpdateProof(
  otp: string,
  storedKey: string,
  cotp: string,
): Promise<string> {
  return sha512Hex(otp + storedKey + cotp);
}

export function constantTimeEqual(left: string, right: string): boolean {
  if (left.length !== right.length) {
    return false;
  }
  const encoder = new TextEncoder();
  return crypto.subtle.timingSafeEqual(
    encoder.encode(left),
    encoder.encode(right),
  );
}

export function formatOtpResponse(otp: string): string {
  // The C server searches for this exact token, including the whitespace.
  return `{"otp": "${otp}"}`;
}

export function randomToken(): string {
  const bytes = new Uint8Array(32);
  crypto.getRandomValues(bytes);
  return bytesToHex(bytes);
}

export function parseUpdatePayload(form: FormData): UpdatePayload {
  const serverId = requiredField(form, "server_id", 64).toLowerCase();
  const quicPort = parsePort(requiredField(form, "quic_port", 5), "quic_port");
  const quicCertSha256 = requiredField(
    form,
    "quic_cert_sha256",
    64,
  ).toLowerCase();
  if (!HEX_64.test(serverId) || !HEX_64.test(quicCertSha256) ||
      serverId !== quicCertSha256) {
    throw new RequestError("Invalid QUIC server identity");
  }

  const quicHostRaw = optionalField(form, "quic_host", 64);
  const quicHost = quicHostRaw === null
    ? null
    : normalizeIpAddress(quicHostRaw);
  const playersCountRaw = requiredField(form, "num_players", 10, true);
  const playersCount = parseUnsignedInteger(
    playersCountRaw || "0",
    "num_players",
    4_294_967_295,
  );
  const registrationRaw = optionalField(form, "registration", 1);
  if (registrationRaw !== null && registrationRaw !== "0" && registrationRaw !== "1") {
    throw new RequestError("Invalid registration marker");
  }

  const cotp = requiredField(form, "cotp", 128).toLowerCase();
  const key = requiredField(form, "key", 128).toLowerCase();
  if (!HEX_128.test(cotp) || !HEX_128.test(key)) {
    throw new RequestError("Invalid authentication value");
  }

  return {
    serverId,
    name: requiredField(form, "name", 80),
    playersCount,
    version: requiredField(form, "version", 32),
    textComment:
      optionalField(form, "text_comment", 256) ?? "No description.",
    otp: requiredField(form, "otp", 256),
    cotp,
    key,
    registration: registrationRaw === "1",
    isPublic: parseBooleanField(form, "public", false),
    quicHost,
    quicPort,
    quicCertSha256,
    passwordRequired: parseBooleanField(form, "password_required", false),
  };
}
function parseBooleanField(
  form: FormData,
  name: string,
  fallback: boolean,
): boolean {
  const value = optionalField(form, name, 5);
  if (value === null) {
    return fallback;
  }
  if (["1", "true", "on"].includes(value.toLowerCase())) {
    return true;
  }
  if (["0", "false", "off"].includes(value.toLowerCase())) {
    return false;
  }
  throw new RequestError(`Invalid ${name}`);
}

function requiredField(
  form: FormData,
  name: string,
  maximumLength: number,
  allowEmpty = false,
): string {
  const value = form.get(name);
  if (typeof value !== "string") {
    throw new RequestError(`Missing field: ${name}`);
  }

  const trimmed = value.trim();
  if ((!allowEmpty && trimmed.length === 0) || trimmed.length > maximumLength) {
    throw new RequestError(`Invalid field: ${name}`);
  }

  return trimmed;
}

function optionalField(
  form: FormData,
  name: string,
  maximumLength: number,
): string | null {
  const value = form.get(name);
  if (value === null || value === "") {
    return null;
  }

  if (typeof value !== "string" || value.length > maximumLength) {
    throw new RequestError(`Invalid field: ${name}`);
  }

  return value;
}

function parsePort(value: string, name: string): number {
  return parseUnsignedInteger(value, name, 65_535, 1);
}

function parseUnsignedInteger(
  value: string,
  name: string,
  maximum: number,
  minimum = 0,
): number {
  if (!/^\d+$/.test(value)) {
    throw new RequestError(`Invalid ${name}`);
  }

  const parsed = Number(value);
  if (!Number.isSafeInteger(parsed) || parsed < minimum || parsed > maximum) {
    throw new RequestError(`Invalid ${name}`);
  }
  return parsed;
}

export function escapeXml(value: string): string {
  return value
    .replace(/[\u0000-\u0008\u000b\u000c\u000e-\u001f]/g, "")
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&apos;");
}

export function ipAddressesEqual(left: string, right: string): boolean {
  return normalizeIpAddress(left) === normalizeIpAddress(right);
}

export function normalizeIpAddress(value: string): string {
  let address = value.trim().toLowerCase();
  if (address.startsWith("[") && address.endsWith("]")) {
    address = address.slice(1, -1);
  }

  const zone = address.indexOf("%");
  if (zone !== -1) {
    address = address.slice(0, zone);
  }

  if (!address.includes(":")) {
    return normalizeIpv4(address);
  }

  const halves = address.split("::");
  if (halves.length > 2) {
    throw new RequestError("Invalid source IP address");
  }

  const left = parseIpv6Words(halves[0]);
  const right = halves.length === 2 ? parseIpv6Words(halves[1]) : [];
  const missing = 8 - left.length - right.length;
  if (
    (halves.length === 1 && missing !== 0) ||
    (halves.length === 2 && missing < 1)
  ) {
    throw new RequestError("Invalid source IP address");
  }

  return [...left, ...Array(missing).fill("0"), ...right]
    .map((word) => word.padStart(4, "0"))
    .join(":");
}

function parseIpv6Words(part: string): string[] {
  if (part === "") {
    return [];
  }

  const raw = part.split(":");
  const words: string[] = [];
  for (const word of raw) {
    if (word.includes(".")) {
      const ipv4 = normalizeIpv4(word)
        .split(".")
        .map((value) => Number.parseInt(value, 10));
      words.push(((ipv4[0] << 8) | ipv4[1]).toString(16));
      words.push(((ipv4[2] << 8) | ipv4[3]).toString(16));
    } else {
      if (!/^[0-9a-f]{1,4}$/.test(word)) {
        throw new RequestError("Invalid source IP address");
      }
      words.push(word.replace(/^0+/, "") || "0");
    }
  }

  return words;
}

function normalizeIpv4(value: string): string {
  const octets = value.split(".");
  if (
    octets.length !== 4 ||
    octets.some(
      (octet) =>
        !/^\d{1,3}$/.test(octet) ||
        Number.parseInt(octet, 10) > 255,
    )
  ) {
    throw new RequestError("Invalid source IP address");
  }

  return octets.map((octet) => String(Number.parseInt(octet, 10))).join(".");
}
