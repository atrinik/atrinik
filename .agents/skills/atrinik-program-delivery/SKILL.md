---
name: atrinik-program-delivery
description: Deliver ordered Atrinik master issues through child work, merge gates, integration review, and final handoff. Explicit `$atrinik-program-delivery` invocation only.
---

# Deliver an Atrinik program

Treat an ordered master issue as one durable objective across its leaf
deliveries and external merge gates.

After an exact leaf ledger records authority, invocation permits claims/Project
updates, ordinary pushes, issue-mode drafts, exact PR updates, binding, and
gated readiness. It forbids force-push, merge/closure, bypass, destructive
reset, cleanup, self-approval, unledgered child/link creation, generic issue
publication, and unrelated changes. Incidental coordinates stay read-only;
"finish" never grants merge authority.

## Load the delivery contracts

1. Follow the complete
   [issue-delivery contract](../atrinik-issue-delivery/SKILL.md) in explicit
   issue mode for every leaf. All exit conditions remain; this adds orchestration
   and never delegates PR-mode adoption.
2. Load `atrinik-multi-repo-workspace`, `atrinik-github-governance`, and the
   narrow implementation, runtime, or scenario skills selected by each leaf.
3. Read the [program checklist](references/program-review-checklist.md) before
   audits and the
   [master-publication ledger contract](references/master-publication-ledger.md)
   before master-comment or missing-child mutation.
4. Copy [the program report template](assets/program-delivery-report.md) to
   ignored `<workspace>/build/program-delivery/<coordinate-sha256>.md`. Prove the
   destination is ignored before writing. Preserve and update it across
   resumptions; never delete it implicitly. It is human-only; the master ledger
   and schema-v1 leaf sidecars remain authoritative.

## Establish the live program

If and only if the user explicitly requested `/goal` or another persistent
goal, create or resume one goal continuously owning the exact master and leaves
before live preflight. Each leaf sidecar records its goal proof, allowed
master/leaf node IDs, distinct coordinates, and exclusive `leaf_position`; the
leaf is explicit. Never create a nested or per-leaf goal. Without durable goal
authority, keep summaries and proposed children local; create no master ledger,
master comment, child, or link.

1. Normalize the master as `owner/repository#number`. Inspect the live issue,
   body, comments, native parent/subissue graph, linked PRs, assignees, Project
   item, releases, repository policy, and current branch tips before mutation.
   Use the requested GitHub access path and never expose credentials.
2. Treat hierarchy as ownership, not sequencing. Derive order only from the
   master's explicit execution plan, dependency statements, and technical
   constraints. Reject cycles, contradictory owners, repository mismatches,
   ambiguous acceptance, or a closed master until reconciled.
3. Verify completion from merged coordinates, current content/checks, or an
   explicit maintainer attestation recorded as such; never invent evidence.
4. Matrix owner issue, repository/line, dependencies, branch/PR/head,
   acceptance, validation, and next gate. Reuse issues. Reuse artifacts only
   when the exact leaf's schema-v1 ledger records created/adopted state after
   helper-complete migration, or bind its unique pre-recorded pending match
   before mutation. Other PRs block or stay read-only until separately
   authorized. If a live search proves a child owner is missing, record the exact
   proposed child/link as a read-only handoff pending a program-level ledger.
5. Before claiming the master or a leaf, create or complete migration of the
   first ready leaf sidecar with exact master/leaf/position and goal authority.
   Then claim idempotently: assign `zoeyrose`, add the issue to **Atrinik work**
   when needed, and set existing Status to **In progress** from
   `github-settings/config/planning.json`. Never invent an `in-progress` label.

A merge-ready leaf is progress, not goal completion.

## Deliver ready stages

1. Select a dependency-ready stage in declared order. Parallelize only work
   sharing no base, schema, ledger, generated baseline, authored/closing path.
2. Apply issue delivery in issue mode through implementation, leaf whole-diff
   convergence, and validation, but hold its final check/readiness transition
   until step 5. Record exact master/leaf program context. Resume only that
   leaf's schema-v1 ledger after helper-complete migration, or bind its exact
   pending match before mutation; never create a replacement. If fresh issue-mode
   preflight finds a different active PR, stop with its coordinate; program
   delegation does not authorize switching to PR mode or adopting it. For
   paired release lines, keep separate bases, commits, validation, and PRs;
   preserve the declared merge order and canonical issue-closing path. Do not
   mark a draft ready until both its leaf review and the cumulative program
   review below converge.
3. Reconcile every head onto its current required base before relying on
   validation. Before target drift, CAS-cancel leaf body/readiness/planned-comment
   intents; recover an in-flight leaf comment or stop, then refresh/replan. A
   prior green check proves only the old head/base combination. Never bulk-merge
   or bulk-refresh a queue whose earlier merges change later baselines.
4. Run leaf whole-diff reviews to convergence, then review the cumulative
   program state against already merged work, other in-flight heads, master
   invariants, and the program checklist. Give findings stable program IDs, fix
   every actionable item, rerun affected validation, and record exact reviewed
   heads in the persistent report.
5. After both leaf and cumulative reviews converge, run issue delivery's
   latest-head/check/readiness section completely. Update only ledger-owned leaf
   PR bodies when durable state changes; generic master and leaf issue bodies
   remain read-only. Publish the master summary only through the master ledger's
   bound comment; verify rendering and avoid noise.

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
Record the resulting suffix SHAs in the ignored report/final handoff after merge.
Never require a commit to contain its own immutable hash.

If the audit finds actionable work, return it to its existing owner. If the
duplicate search proves no owner exists, record a proposed child as a read-only
handoff pending program-ledger authority. Once an authorized child exists,
deliver that leaf through its review and merge gate, refresh the live graph and
tips, then repeat the terminal audit. Never waive a stage or finding yourself.

Update the master ledger's bound comment with exact final coordinates; do not
close the issue. The final handoff names any remaining closure or release action
and the preserved report path. Mark the durable goal complete only when the
master is genuinely ready to close, not merely because the current stage is
ready or waiting at a merge gate.

Program completion grants no cleanup authority. Use issue delivery's terminal
contract per merged leaf; never archive a master-referenced ledger.
