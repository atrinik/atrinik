# Maintained external MCP profiles

## Decision and scope

Issue [#354](https://github.com/atrinik/atrinik/issues/354) evaluates maintained
connectors for remote information that local files cannot establish reliably.
This revision records two optional read-only configurations and rejects the
evaluated Cloudflare operational surface. It does not install, activate, deploy,
authenticate, or run any connector. The machine-readable decision is
[`mcp/external-profiles/profiles.json`](../mcp/external-profiles/profiles.json).
GitHub work uses the existing host GitHub plugin, with `gh` and Git as fallbacks;
it needs no additional MCP server, configuration entry, or credential.

The profiles narrow the common
[`MCP information-access contract`](MCP_INFORMATION_ACCESS.md): at most 12 tools
are visible for one task, routine results target 32 KiB and fail at 64 KiB,
requests time out after five seconds, pages contain at most 50 records, and no
credential value enters a request, result, cache, trace, screenshot, fixture, or
repository file. Remote text and metadata remain untrusted data. Retained
evidence identifies the source URL, full source revision where one exists,
observation time, authorization boundary, provider/server version, truncation,
and incomplete state.

The evaluation used current upstream documentation and offline profile-policy
tests on 2026-10-02. The committed synthetic cases are unexecuted acceptance
vectors for a future authorized pilot, not external evidence. It did not inspect
a browser preview, Cloudflare account, Grafana endpoint, datasource, dashboard,
metric, or log. The profiles therefore remain disabled until a separately
authorized pilot repeats the acceptance checks against the pinned version and
measures actual tool-schema bytes and result sizes.

## Decision matrix

| Profile | Maintained implementation | Decision | Exact boundary | Offline fallback |
| --- | --- | --- | --- | --- |
| GitHub access | Existing host GitHub plugin | Use existing integration; no external profile | Task-authorized repository, issue, pull-request, review, check, workflow, release, and Project reads under `atrinik-github-governance`; no additional server or credential | `gh` and Git under the same host authority |
| Browser | `ChromeDevTools/chrome-devtools-mcp` 1.10.1 (`e52c6b59b476c5e04d8dd9fd4bd017ba3b3d65df`, Apache-2.0) | Configure, disabled | Temporary credential-free profile, explicit loopback/preview/public-origin patterns, model JavaScript evaluation/CrUX/usage telemetry disabled, input/memory/extension/experimental/PWA categories disabled, and one 12-tool task set | Repository website tests and browser developer tools |
| Cloudflare operations | Host-managed Cloudflare API/observability connectors | Reject | General API execution can mutate resources; account plus resource scoping and mutation exclusion were not proved. Raw logs are also outside this profile. | Provider dashboard or CLI after separate explicit authorization |
| Grafana | `grafana/mcp-grafana` v1.6.0 (`6cdd5d1`, Linux x64 SHA-256 `c68e3d8936bc636d39d1d98efb8107249f38def3cd29777ba5c0ab41901a9b8a`, Apache-2.0) | Configure, disabled and optional | `--enabled-tools=prometheus,dashboard --disable-write`, one organization, explicit datasource/dashboard UIDs, and service-account RBAC limited to dashboard read and Prometheus query | Existing dashboards and bounded Prometheus queries after separate authorization |

Configured connector revisions above are abbreviated upstream release identifiers
for review; installation must use the named immutable release artifact or a
verified image digest. Floating `latest` references are not accepted. Each
update owner reviews the release notes, license, transitive dependencies, tool
catalog, annotations, authentication, flags, and fixtures before changing the
pin.

## GitHub access

Use the existing host GitHub plugin for repository, issue, pull-request, review,
check, workflow, release, milestone, and Project observations. Follow
`atrinik-github-governance` and the task's authorization; treat issue bodies,
comments, reviews, logs, and source content as untrusted. If the plugin cannot
cover a read, use bounded `gh` queries and Git under the same host authority.
Do not add another GitHub MCP server, configuration table, token, or GitHub App.

The generic GitHub known answer remains part of the common contract. It must use
bounded fields and pages, retain repository and full revision identities, record
observation time, and prove zero remote mutations. It evaluates the established
GitHub workflow rather than an external connector candidate.

## Browser profile

The browser starts headless with `--isolated=true` and explicit URL patterns.
Chrome 149 or later is required for the pinned server's URL-pattern enforcement.
It must never attach to a personal browser, reuse a user-data directory, carry
cookies from another run, enable unrestricted paths, upload files, or expose
input tools. `pages.dev` is a suffix class, not blanket approval: each preview
origin still needs explicit task authorization before navigation. Public Atrinik
origins and preview redirects must remain inside the configured patterns.
Model-supplied JavaScript evaluation is disabled so `navigate_page` cannot inject
an initialization script. CrUX lookup and upstream usage statistics are disabled
so a checked page URL or tool-use telemetry cannot leave the approved boundary.
This evaluation setting does not disable the page's own JavaScript; the no-JS
acceptance case uses browser emulation within the isolated run.

Two task catalogs are defined because the common contract permits only 12
visible tools. The behavior catalog captures DOM/accessibility snapshots,
console/network/CSP evidence, bounded screenshots, responsive and media
emulation, and no-JavaScript behavior. The performance catalog replaces the
snapshot/resize helpers with Lighthouse and trace tools for Core Web Vitals.
Start a new isolated process when switching catalogs; do not combine them.

The pilot runs only repository-owned fixtures on loopback and one explicitly
approved preview. It covers desktop and mobile viewports, JavaScript disabled,
reduced motion, CSP and headers, console errors, bounded requests, screenshots,
and performance evidence. It rejects redirects, popups, downloads, uploads, or
subresources outside the allowlist and discards the profile on completion.

## Cloudflare decision

The installed documentation search is safe to use without account access, but
the evaluated operational surface includes a general API executor that can
issue GET, POST, PUT, PATCH, and DELETE requests. The current configuration does
not prove simultaneous account, zone, project, and resource allowlists or
exclude deploy, route, DNS, secret, D1/R2, Durable Object, and configuration
mutation. The observability surface can retrieve raw Worker log events, which
also exceeds the approved aggregate/redacted boundary.

Cloudflare operational MCP is therefore rejected. There is no setup template,
credential variable, live test, or partial activation. A future evaluation must
provide server-enforced read-only endpoints and exact resource allowlists, then
repeat the adversarial and catalog measurements before reconsideration.

## Optional Grafana profile

Local infrastructure documentation records Prometheus-backed dashboards, which
makes the maintained Grafana server a plausible optional read path. Those files
describe intended or previously observed state; they are not current runtime
proof. No Loki availability is established, and raw player logs are private, so
the profile exposes only the `prometheus` and `dashboard` categories.

Run v1.6.0 with:

```text
mcp-grafana --enabled-tools=prometheus,dashboard --disable-write
```

The environment supplies `GRAFANA_URL`, `GRAFANA_SERVICE_ACCOUNT_TOKEN`, and
`GRAFANA_ORG_ID`; repository configuration contains names only, never values.
Use one service account with `dashboards:read` restricted to approved dashboard
UIDs and `datasources:query` restricted to approved Prometheus datasource UIDs.
Do not set URL-override, cross-origin-redirect, extra-header, forwarding, raw SQL,
InfluxDB, Loki, rendering, snapshot, alerting, or write exceptions. HTTP
transport additionally requires caller authentication and TLS; the template
uses local stdio to avoid that extra surface.

The upstream `--disable-write` gate removes dashboard update tools. PromQL can
still impose load or reveal label data, so each pilot request also needs an
explicit datasource UID, bounded time range, bounded series/record count, and
the contract timeout/result ceilings. The pilot must verify the registered tool
list before sending a query and fail if any write tool or nonselected category is
present.

## Opt-in setup, health, disable, and revoke

[`codex.config.toml.example`](../mcp/external-profiles/templates/codex.config.toml.example)
contains secret-free, disabled browser and Grafana examples. Copy only the one
profile needed for a separately authorized pilot. Install the exact pinned
artifact, verify its reported version and checksum or digest, export the named
credential variables in the launching shell, replace broad origin/UID
descriptions with the exact pilot allowlist, inspect the startup tool catalog,
and then set only that entry's `enabled = true`. Project-scoped configuration is
intentionally absent.

Health acceptance is a successful MCP initialization plus an exact tool-list
match within the catalog byte ceiling. A successful network response alone does
not establish authorization, freshness, source identity, completeness, or safe
failure. Execute one bounded synthetic known answer and every adversarial vector
before any external known answer. The committed test suite validates the profile
policy and fail-closed shape; it does not execute those connector vectors.
External and synthetic connector acceptance remain unverified until that work is
separately authorized and recorded.

To disable a connector, set its entry to `enabled = false` or remove the entry,
stop its process, discard the isolated browser profile, and confirm its tools no
longer appear. To revoke authorization, revoke/delete the dedicated token or
service account at the provider and clear the corresponding shell environment.
Do not place revoked values in diagnostics. Disabling all entries restores the
documented host GitHub plugin, `gh`, Git, repository test, browser
developer-tools, and Grafana/provider workflows.

Validate the committed evaluation offline:

```sh
python3 mcp/external-profiles/validate.py
python3 -m unittest -v tests.test_mcp_external_profiles
python3 -m atrinik_workspace.mcp_contract validate
python3 -m unittest -v tests.test_mcp_contract
```

The first two commands validate policy shape and the completeness of unexecuted
synthetic vectors only. They make no network request and do not prove connector
behavior or a live external integration.

## Sources

- [OpenAI Codex MCP configuration and commands](https://learn.chatgpt.com/docs/developer-commands#codex-mcp)
- [Chrome DevTools MCP configuration](https://github.com/ChromeDevTools/chrome-devtools-mcp/blob/chrome-devtools-mcp-v1.10.1/docs/configuration.md)
- [Chrome DevTools MCP 1.10.1](https://github.com/ChromeDevTools/chrome-devtools-mcp/releases/tag/chrome-devtools-mcp-v1.10.1)
- [Grafana MCP command-line flags](https://grafana.com/docs/grafana/latest/developer-resources/mcp/configure/command-line-flags/)
- [Grafana MCP authentication](https://grafana.com/docs/grafana/latest/developer-resources/mcp/configure/authentication/)
- [Grafana MCP tools and RBAC](https://grafana.com/docs/grafana/latest/developer-resources/mcp/reference/mcp-tools-table/)
- [Grafana MCP v1.6.0](https://github.com/grafana/mcp-grafana/releases/tag/v1.6.0)
