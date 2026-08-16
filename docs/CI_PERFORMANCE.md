# CI performance

The required pull-request check remains `Integration validation`. Its latency
budget is a median of at most 5 minutes and a p95 of at most 7 minutes across at
least 20 comparable successful GitHub-hosted runs. Comparable runs use the same
head commit, `ubuntu-24.04`, Python 3.13, the complete discovered test set,
branch coverage, and the same workflow definition. Percentiles use the
nearest-rank method over job elapsed time from start to completion.

## Baseline

Before sharding, the workflow ran every test and validator serially. The latest
pre-change main sample took 7m04s, including 6m48s in the combined validation
step and 400.556s in `unittest`. The latest ten successful jobs had a 7m53s
median and a 5m43s–8m33s range. A clean local run at
`8e0b5fdf38a18bff3bec99a3a111dea3f783fff4` discovered 1,180 tests and took
467.353s under Python 3.14; the sandbox denied topology sockets, so that local
run supplies completed-test timing weights rather than pass/fail evidence.

The dominant `WorkspaceTests` fixture previously ran 13 Git commands for each
of seven repositories before every method. At the 415-method issue baseline,
that was 37,765 fixed setup subprocesses. The fixture now builds one immutable
class template with 15 Git commands per repository and copies it without Git
subprocesses for each method. Three independent CI shards therefore perform
315 fixed template commands in total, a 99.2% reduction, while every test keeps
private writable refs, indexes, configs, origins, worktrees, permissions,
paths, locks, and teardown.

Seven post-change local setup/teardown samples were 0.078, 0.078, 0.078,
0.078, 0.080, 0.081, and 0.134 seconds (0.078s median), compared with the
0.431s pre-change median recorded for the issue: an 81.8% median reduction.
A post-change serial run discovered 1,184 tests and passed in 400.620s. That
run refreshes the retained slow-test weights; the pre-change metadata remains
in the timing file so the source and limitations of the baseline stay explicit.

## Required workflow

Three process-isolated jobs discover the complete suite independently and use
longest-processing-time assignment from `ci/test-timing-weights.json`. Exact
slow-test observations override measured class averages; deterministic test-ID
ordering breaks ties. Every shard retains a machine-readable assignment
manifest, all per-test durations, and parallel coverage data for 14 days.

The aggregate job fails unless all shard jobs passed, rediscovery matches every
manifest, and the union contains every test exactly once with no duplicates.
It then combines branch coverage once, runs compile, guidance, MCP, manifest,
provenance, and supply-chain validation as visible steps, and performs the sole
Codecov upload. Pull-request-scoped concurrency cancels superseded heads.

## Hosted evidence

Record each comparable run's URL, aggregate elapsed time, longest shard,
combined runner minutes, and queue delay. Runner minutes are the sum of elapsed
minutes for all shard and aggregate jobs, so wall-clock improvement cannot hide
unbounded parallel cost. Update the retained timing weights only from complete,
successful, comparable data; keep old observations in Git history.

The delivery pull request for issue #463 records its 20-run sample and final
median, p95, queue behavior, and runner-minute comparison in its delivery-owned
status section before readiness.
