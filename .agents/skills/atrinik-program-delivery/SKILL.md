---
name: atrinik-program-delivery
description: Deliver ordered Atrinik master issues through child work, merge gates, integration review, and final handoff. Explicit `$atrinik-program-delivery` invocation only.
---

# Deliver an Atrinik program

Use this skill for a master issue whose acceptance depends on an ordered set of
child issues, release-line pairs, or follow-up audits. Treat the program as one
durable objective across multiple leaf deliveries and external merge gates.

Explicit invocation authorizes assignment and Project updates for the master,
ordinary branch pushes, child issue and draft PR creation when genuinely
missing, updates to linked issues and to PRs created and recorded by the exact
delegated issue-mode leaf, ready transitions after exit conditions, and concise
tracking comments. It does not authorize force-pushes, merges, issue closure,
policy bypass, destructive resets, cleanup application, self-approval, or
unrelated external changes. Do not infer merge authority from requests to
finish, complete, or fully deliver a program.

## Load the delivery contracts

1. Read and follow the complete
   [issue-delivery contract](../atrinik-issue-delivery/SKILL.md) for every leaf,
   selecting its explicit issue mode. Its validation, review, publication, and
   ready-state exit conditions remain mandatory; this skill adds orchestration
   rather than replacing them and does not delegate PR-mode adoption.
2. Load `atrinik-multi-repo-workspace`, `atrinik-github-governance`, and the
   narrow implementation, runtime, or scenario skills selected by each leaf.
3. Read the full
   [program review checklist](references/program-review-checklist.md) before
   planning and again before the final audit.
4. Copy [the program report template](assets/program-delivery-report.md) to an
   ignored path such as
   `<workspace>/build/program-delivery/<owner>-<repo>-<number>.md`. Prove the
   destination is ignored before writing. Preserve and update that report
   across resumptions; never delete it implicitly.

## Establish the live program

If and only if the user explicitly requested `/goal` or another persistent
goal, create or resume one goal for the whole master before live preflight.
Never create a nested or per-leaf goal. A preflight finding remains part of that
objective rather than requiring a fresh goal on resumption.

1. Normalize the master as `owner/repository#number`. Inspect the live issue,
   body, comments, native parent/subissue graph, linked PRs, assignees, Project
   item, releases, repository policy, and current branch tips before mutation.
   Use the requested GitHub access path and never expose credentials.
2. Treat hierarchy as ownership, not sequencing. Derive order only from the
   master's explicit execution plan, dependency statements, and technical
   constraints. Reject cycles, contradictory owners, repository mismatches,
   ambiguous acceptance, or a closed master until reconciled.
3. Verify every claimed completion from merged coordinates, current content,
   checks, or an explicit maintainer attestation. Record attestations as such;
   do not invent evidence.
4. Build a stage matrix containing owner issue, target repository and release
   line, dependencies, existing branch/PR/head, acceptance, validation, and the
   next human gate. Reuse existing issues. Reuse worktrees, branches, and PRs
   only when they were created and recorded by that exact delegated issue-mode
   leaf. Every other existing PR is a blocker or read-only traceability until
   separately authorized. Create and link a child only when required work has
   no current owner after a live duplicate search.
5. Apply the issue-delivery claim contract to the master: assign `zoeyrose`, add
   it to **Atrinik work** when needed, and set the existing Status to
   **In progress** from `github-settings/config/planning.json`. Never invent an
   `in-progress` label. Let issue delivery claim each ready leaf.

A merge-ready leaf is progress, not goal completion.

## Deliver ready stages

1. Select only a dependency-ready stage. Follow the master's stated order even
   when later PRs are already green. Parallelize only independent work with no
   shared base, schema, ledger, generated baseline, authored path, or closing
   path.
2. Apply the issue-delivery workflow completely to the leaf in issue mode.
   Resume only work created and recorded by that exact leaf delivery rather
   than creating a replacement PR. If fresh issue-mode preflight finds a
   different active PR, stop with its coordinate; program delegation does not
   authorize switching to PR mode or adopting it. For paired release lines,
   keep separate bases, commits, validation, and PRs; preserve the declared
   merge order and canonical issue-closing path. Do not mark a draft ready until
   both its leaf review and the cumulative program review below converge.
3. Reconcile every head onto its current required base before relying on
   validation. A prior green check proves only the old head/base combination.
   Never bulk-merge or bulk-refresh a queue whose earlier merges change later
   baselines.
4. Run leaf whole-diff reviews to convergence, then review the cumulative
   program state against already merged work, other in-flight heads, the master
   invariants, and the program checklist. Give findings stable program IDs,
   fix every actionable item, rerun affected validation, and record the exact
   reviewed heads in the persistent report.
5. Update the master matrix and relevant issue/PR bodies when durable state
   changes. Keep comments concise, preserve history, verify remote rendering,
   and avoid progress noise.

## Stop at every merge or human gate

Before yielding, confirm that each proposed PR is at the reviewed head, is
mergeable, has current required checks, has no unresolved actionable feedback,
and uses the intended closing reference. Report:

- exact PR numbers, head SHAs, bases, and merge order;
- checks and local validation at those heads;
- any required manual scenario, policy decision, or post-merge action; and
- which next stages will become stale or unblocked after the gate.

Do not merge or close anything, and do not mark the program goal complete at a
gate. Wait without busy-polling and follow the environment's active/blocked
goal lifecycle; an unchanged external gate becomes blocked only under its
required recurrence threshold. When work resumes, query GitHub rather than
trusting the reported action: verify merged commits, issue state, new branch
tips, checks, graph, and comments. Replan from that state. A merge on one
release line invalidates only the dependent evidence, but every affected
descendant must be reconciled and revalidated before its gate.

## Finish the program

After all implementation stages have been externally merged or explicitly
excepted by the master or maintainer, run the master's terminal audit on the
exact final tips. Review the complete resulting state rather than summing old
PR reviews. Confirm that:

- every acceptance item has a verified owner and outcome;
- child and closing states match the declared release-line policy;
- required ledgers, baselines, generated consumers, and branch invariants are
  current, with no orphaned or duplicate work;
- final validation and latest-head required checks pass; and
- no known actionable leaf or program finding remains.

When a terminal ledger or audit must account for commits it creates, record an
immutable pre-ledger horizon for each line before the first terminal merge.
Predeclare the exact ordered count of remaining squash commits and an exact
changed-path allowlist for each ordinal. At the final tip, require the suffix
length, order, and paths to match that declaration; any missing, extra,
reordered, or out-of-allowlist commit invalidates it and requires a new horizon.
Record the resulting suffix SHAs durably in the master and report after merge.
Never require a commit to contain its own immutable hash.

If the audit finds actionable work, return it to its existing owner or create a
child only when the duplicate search proves none exists. Deliver that leaf
through its review and merge gate, refresh the live graph and tips, then repeat
the terminal audit. Never waive a stage or finding yourself.

Update the master with exact final coordinates and a concise close-ready
summary, but do not close it. The final handoff names any remaining human
closure or release action and the preserved report path. Mark the durable goal
complete only when the master is genuinely ready to close, not merely because
the current stage is ready or waiting at a merge gate.
