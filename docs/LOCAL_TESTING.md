# Local test execution

The wrapper test suite can run in process-isolated shards through the same
deterministic timing assignment used by CI. Run the commands from the wrapper
root after installing `requirements-dev.txt`.

## Supported commands

Run a targeted test quickly without coverage:

```sh
python3 -m atrinik_workspace.ci_sharding local \
  --jobs 1 \
  --test tests.test_ci_sharding.CiShardingTests.test_assignment_is_deterministic_and_uses_measured_weights
```

Run the complete suite serially (the explicit fallback for debugging):

```sh
python3 -m atrinik_workspace.ci_sharding local --jobs 1
```

Run the complete suite in parallel without coverage:

```sh
python3 -m atrinik_workspace.ci_sharding local --jobs 3
```

Run the complete suite with branch coverage:

```sh
python3 -m atrinik_workspace.ci_sharding local --jobs 3 --coverage
```

Omit `--jobs` to choose a conservative default of at most three processes from
the available CPUs. Every worker is a separate Python process. The parent
discovers the selected tests once, writes immutable per-shard assignments, and
requires every selected test to report exactly once before declaring success.

Coverage mode gives every worker a unique data-file prefix, combines the
completed files exactly once, and writes the report into the retained run
directory. Each invocation creates a unique directory below
`build/local-tests/`; its `run.json`, shard logs, manifests, timings, and
coverage evidence are the recovery record. Interrupted or failed runs are
marked accordingly and keep their diagnostics so a later run cannot silently
consume stale coverage.

The local command is the test runner only. For CI-parity validation, use the
repository's documented aggregate commands after it passes:

```sh
python3 -m compileall -q atrinik atrinik_workspace tests
python3 -m atrinik_workspace.guidance_inventory --check
python3 -m atrinik_workspace.mcp_contract validate
./atrinik manifest validate
./atrinik provenance validate
./atrinik supply-chain validate
```

The hosted workflow remains authoritative for the required `Integration
validation` check and uses the same `ci/test-timing-weights.json` assignment.

