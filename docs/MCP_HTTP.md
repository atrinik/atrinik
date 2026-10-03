# Native remote MCP

`atrinik_workspace.mcp_http` exposes the existing seven context/search tools
through authenticated Streamable HTTP at `/mcp`. Codex can connect directly to
an HTTPS URL; no local stdio process, bridge, or client package is required.
The source provider and its selectors, schemas, source fencing and read limits
are unchanged. Runtime observation and external content providers remain
separate opt-ins and are not advertised by this server.

## Docker Compose quickstart

The supported remote deployment uses the published public-source image and
`deploy/mcp/compose.yaml`; direct host Python startup is only an internal
diagnostic. Before starting, point public DNS for the chosen hostname at this
host, allow inbound TCP ports 80 and 443, leave those ports free for Caddy, and
allow outbound ACME connectivity for certificate issuance.

The initial `linux/amd64` image was built from wrapper commit
`476cf9dad436ce7b5fb89113c46014fcca3b8f77`. Anonymous manifest lookup, pull and
the seven-tool smoke test passed for this exact digest. Replace the example
hostname with the public deployment hostname:

From the wrapper checkout, start with two commands:

```sh
export MCP_DOMAIN=mcp.example.invalid MCP_IMAGE=ghcr.io/atrinik/atrinik-mcp@sha256:e1e8880cc80979813e9ef39fcfe4b6568a7cab48124bbf3b0f02f72b327c4618
docker compose -f deploy/mcp/compose.yaml up -d
```

`mcp.example.invalid` remains a hostname placeholder. Set the same values in
`deploy/mcp/.env` if the deployment will be managed by later Compose invocations
without exported variables. The image contains the immutable public source view
and needs no host source mount, runtime clone or update. Do not add player state,
private repositories, personal configuration, Docker sockets, SSH agents or
host credentials.

The initialization service creates the bearer token in its private Docker
volume. Retrieve it deliberately from a trusted terminal:

```sh
docker compose -f deploy/mcp/compose.yaml exec -T mcp cat /var/lib/atrinik-auth/token
```

This command intentionally prints the token. Do not redirect it, pipe it through
logging or paste it into shell history. Copy it into a hidden prompt on the Codex
host, keep that environment variable available whenever Codex starts, then add
the native Streamable HTTP URL:

```sh
read -rsp 'Atrinik MCP bearer token: ' ATRINIK_MCP_TOKEN; echo
export ATRINIK_MCP_TOKEN
codex mcp add atrinik --url "https://mcp.example.invalid/mcp" \
  --bearer-token-env-var ATRINIK_MCP_TOKEN
```

Codex reads the secret from the named environment variable and sends it as a
bearer token; the value does not belong in `config.toml`. This is the native URL
and authentication form documented by the official
[OpenAI Streamable HTTP MCP guide](https://learn.chatgpt.com/docs/extend/mcp#streamable-http-servers).

## Authentication and source boundary

The Compose initialization service generates a cryptographically random
64-character URL-safe token as a mode-0600 regular file in the private `auth`
volume. The MCP service loads it at startup. Never put the token into a command
argument, source control, an image, a build argument or logs. Caddy obtains and
renews the public certificate through ACME; clients must trust its issuing CA.
The public hostname must continue resolving to this host for TLS renewal.

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

Release maintainers build the public image from the repository root using
`deploy/mcp/official.Dockerfile`. Its default Python image is digest pinned. Any
`PYTHON_IMAGE` override must retain an immutable `@sha256:` reference and be
reviewed before deployment. Git and ripgrep are installed in the image; no
package installation occurs on service startup. The initial published digest
passed anonymous manifest lookup, pull and runtime smoke checks.

Run as UID/GID 10001, with a read-only root filesystem, all Linux capabilities
dropped, `no-new-privileges`, explicit CPU/memory/PID limits, and only the
private token volume. The public source corpus is part of the read-only image.
Only the Caddy proxy publishes ports 80 and 443; the MCP container remains on
the private backend network. Its health check validates the internal endpoint
before Compose starts the proxy. Caddy retains certificate state in its
dedicated volume and does not enable access logging.

The service owner manages deployment, trust installation and restart authority.
Source changes alone do not authorize modifying other services or merging a PR.

For internal transport tests only, maintainers may invoke
`python3 -B -m atrinik_workspace.mcp_http` from a trusted checkout with explicit
test root, token and TLS arguments. That library binding is not a supported
public deployment path.
