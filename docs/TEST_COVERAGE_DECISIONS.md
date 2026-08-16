# Test coverage decisions

Issue #470 requires an evidence-based audit of redundant or obsolete tests. The
initial audit found no safe removals: similarly shaped tests exercise distinct
failure modes, state transitions, or ownership boundaries, and the existing
test IDs remain useful for exact-once sharding and actionable failures.

This delivery therefore removes no tests. It adds process-isolated local
execution, preserves the existing CI shard contract, and keeps all current
fixtures and real topology/Git/concurrency proofs in place. Any future removal
must record the replaced contract, focused mutation or equivalent evidence, and
the retained test IDs in this decision log.

