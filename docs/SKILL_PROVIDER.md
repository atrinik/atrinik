# Agent skill provider

Reusable Atrinik workflows are supplied by the public
[agent-integrations repository](https://github.com/atrinik/agent-integrations), marketplace
`atrinik`, plugin `atrinik-development`. Install that plugin in your agent host
using its supported marketplace interface. Installation is host-specific and
never a prerequisite for ordinary wrapper tests, builds, or offline inspection.

The checked-in [descriptor](../.agents/skill-provider.json) pins the repository,
full commit, plugin path, and twelve skills required by this wrapper. Resolve
skill entries under `<path>/skills/<required_skill>/SKILL.md` at exactly that
revision; use the installed plugin's resources for workflow references. A newer
installation is not evidence for the pinned revision. Missing or mismatched
providers must be reported explicitly when invoking a workflow; do not invent a
local `.agents/skills` path or silently fetch/install code. Repository-owned
`AGENTS.md`, docs, manifests, authority gates and operational scripts remain
local and authoritative. The provider does not grant GitHub, runtime or merge
authority.

`python3 -m atrinik_workspace.guidance_inventory --check` validates the local
routes and descriptor offline. External catalog/body metrics are **unmeasured**
unless explicitly supplied with
`--skills-root /absolute/provider/plugins/atrinik-development/skills`. That
integration check requires all twelve declared entries and enforces the existing
catalog, startup, selected-workflow and total-body budgets. The provider may
include additional skills: its complete catalog is tested in the provider repo.
The metrics check measures supplied files; it does not certify their Git origin.

## atrinik-issue-delivery

Use this skill for explicitly selected issues or PRs. Its `references/preparation.md`
and `references/delivery-ledger.md` govern existing bound deliveries;
`references/resource-observation-recovery.md` governs retained-resource recovery;
`references/deep-review-checklist.md` supplies the review protocol.
The operational helper remains `scripts/delivery_ledger.py` in the selected
wrapper revision. Read [source delivery](SOURCE_DELIVERY.md) for standalone goals.

## atrinik-github-governance

Use this skill for PR publication and Git governance. Read its
`references/ssh-signing.md` SSH signing reference for optional host signing.
The wrapper's [contribution policy](../CONTRIBUTING.md) remains authoritative.

## atrinik-project-delivery

Use this skill for parallel project execution. Its `references/coordinator.md`
provides the operator protocol; [project goals](PROJECT_DELIVERY_GOAL.md) retain
the repository's local project contract.

## Other required workflows

The descriptor also declares `atrinik-c-change`, `atrinik-content-change`,
`atrinik-guidance-maintenance`, `atrinik-linux-gpu-qualification`,
`atrinik-multi-repo-workspace`, `atrinik-program-delivery`,
`atrinik-protocol-change`, `atrinik-server-runtime`, and `atrinik-test-scenario`.
The program workflow remains explicit-only. Select each workflow by its installed
plugin name and read its entry before using its supplementary resources.
