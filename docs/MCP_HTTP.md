# Native remote MCP

`atrinik_workspace.mcp_http` exposes the existing seven context/search tools
through authenticated Streamable HTTP at `/mcp`. Codex can connect directly to
an HTTPS URL; no local stdio process, bridge, or client package is required.
The source provider and its selectors, schemas, source fencing and read limits
are unchanged. Runtime observation and external content providers remain
separate opt-ins and are not advertised by this server.

## Server configuration

```sh
python3 -B -m atrinik_workspace.mcp_http \
  --root /srv/atrinik \
  --token-file /run/secrets/atrinik_mcp_token \
  --host 0.0.0.0 --port 8765 \
  --allowed-host mcp.example.invalid:8443 \
  --allowed-host localhost:8765 \
  --allowed-origin https://mcp.example.invalid:8443 \
  --tls-cert /run/secrets/atrinik_mcp_cert \
  --tls-key /run/secrets/atrinik_mcp_key
```

The absolute root is a reviewed public source snapshot with its Git metadata
and manifest component checkouts. Mount it read-only. It is the remote source
view, not a view of a developer's local dirty worktrees. Do not mount player
state, private repositories, personal configuration, Docker sockets, SSH agents
or host credentials. Startup performs no clone, source update or other write.
Unavailable components produce the provider's ordinary unavailable result.

Provision a cryptographically random URL-safe bearer token of 32–256 characters
in the token file (one optional trailing newline). Token and TLS private key
files must be regular files, not symlinks, with no world access or group write;
0400/0440/0600/0640 are appropriate according to ownership. The service UID must
be able to read them. The token is loaded at startup; replace it and restart only
this service to rotate it. Never put the token into a command argument, source
control, an image, a build argument, or logs. TLS key and certificate must be
provided together. TLS requires version 1.2 or newer; encrypted private keys are
not supported. The certificate must cover the service hostname and localhost
for the container health probe. Clients must trust its issuing CA.

HTTP without TLS is available for loopback fixture tests or an isolated trusted
TLS proxy network. Do not expose plaintext bearer authentication on a LAN or
the Internet. Host allowlists match the exact HTTP authority, including a port
when sent by the client. Origin allowlists match exact origins; an absent Origin
is accepted, while an unlisted Origin is denied. No forwarded headers influence
these checks. Requests fail authentication before processing the JSON body.

`GET /healthz` returns only `{"status":"ok"}` after Host/Origin validation; it
requires no token and reads no source. It proves transport readiness, not source
checkout completeness. All MCP operations and session deletion require bearer
authentication. No request body, header, source exception or secret enters an
access log.

## Wire compatibility and limits

The binding follows the MCP
[Streamable HTTP transport](https://modelcontextprotocol.io/specification/2025-11-25/basic/transports)
and [lifecycle](https://modelcontextprotocol.io/specification/2025-11-25/basic/lifecycle).
Clients initialize with `2025-06-18` or `2025-11-25`, then send the initialized
notification and the returned `MCP-Session-Id` on subsequent requests. The
negotiated `MCP-Protocol-Version` header is checked; omission can use the
session's negotiated version. Unsupported or mismatching versions return 400.
The legacy codec adapts the wire format to the unchanged modern provider.
It preserves the MIT Atrinik Project attribution of the original integration
codec.

POST accepts a single UTF-8 JSON-RPC message with `Content-Type: application/json`
and an Accept header listing `application/json` and `text/event-stream`.
Responses are JSON; accepted notifications return 202 with an empty body.
GET on an authenticated session returns 405 because unsolicited SSE is not
provided. DELETE returns 204 and cancels the session's operations. Missing
sessions return 400; unknown, deleted or expired sessions return 404.
Unsupported client responses are rejected with 400 because this server issues
no server-to-client requests. Duplicate security headers, duplicate JSON keys,
nonfinite numbers, batch messages, chunked requests and oversized bodies are
rejected.

The `2026-07-28` HTTP binding follows its
[stateless transport](https://modelcontextprotocol.io/specification/2026-07-28/basic/transports/streamable-http).
Every request requires the protocol version and method header mirrors and the
provider's version/capability metadata; tool calls and resource reads also
require `Mcp-Name`. Base64 sentinel names are decoded before comparison. Missing
or mismatching headers return 400/-32020; unknown methods return 404/-32601.
Discovery is optional and creates no protocol session. Its resource registry is
bounded to 128 references under the configured bearer principal: modern clients
sharing that credential share the same authorized public source scope. Modern
requests have independent cancellation events; legacy cancellation notifications
cannot affect them. This JSON response binding provides no SSE and accepts no
modern cancellation notifications. The original stdio binding is unchanged.

There are at most 32 authenticated sessions, each expiring after 15 minutes
idle, 16 HTTP connections and four active inspection operations across the
server. Legacy sessions own independent providers, authorization identities, up to 128
resource references, and cancellation maps. One session cannot read another's
issued resources or cancel its requests. Exhausted operation/session capacity
returns 503 with Retry-After; excess connections close before worker creation.
TLS, headers and body receipt share a five-second wall clock ingress budget.
After body receipt, processing has the provider's five-second deadline plus a
one-second allowance to serialize and send its result or structured timeout.
Each response also has a one-second wall clock output limit; the complete
connection is capped at eleven seconds. Transport expiry cancels active work
and shuts down the socket, including stalled TLS handshakes and output. Requests
are limited to 16 KiB, routine responses to 32 KiB and resource reads to 64 KiB.
There is no persisted session cache, replay log or background indexing.

## Container deployment

Build from the repository root using `deploy/mcp/Dockerfile`. Its default Python
image is digest pinned. Any `PYTHON_IMAGE` override must retain an immutable
`@sha256:` reference and be reviewed before deployment. Git and ripgrep are
installed in the image; no package installation occurs on service startup.

Run as UID/GID 10001, with a read-only root filesystem, all Linux capabilities
dropped, `no-new-privileges`, explicit CPU/memory/PID limits, and only the
read-only public source and dedicated token/TLS file mounts described above.
Publish only the selected HTTPS port. The health check validates TLS against
`/run/secrets/atrinik_mcp_ca`, which contains the public CA certificate, and
connects to `localhost:8765`. Provision the CA private key separately from the
container; only its public certificate belongs in this mount.

The service owner manages deployment, trust installation and restart authority.
Source changes alone do not authorize modifying other services or merging a PR.
