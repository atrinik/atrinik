# Legacy server clock domains

The legacy server has three explicit clock domains. New code must store typed
absolute deadlines from `server/src/include/server_clock.h` and must not compare
values from different domains.

## Domain contract

### Simulation ticks

`server_tick_t` is authoritative gameplay time. It advances once in
`main_process()` and therefore advances only when the simulation advances.
Gameplay delays, spell durations, AI schedules, action wind-ups, encounter
expiry, and other deterministic mechanics belong here. The current main-loop
cadence and catch-up behavior remain owned by `reset_sleep()` and
`sleep_delta()`; the clock service observes that cadence but does not change it.

`global_round_tag` is a same-round generation marker for damage and map-update
deduplication. It is not a duration clock. The authored world time-of-day in
`server/src/server/time.c` is also separate: it maps simulation progress to the
game world's calendar and persists `todtick` in `clockdata`.

### Monotonic process time

`server_monotonic_t` is backed by `datetime_monotonic_us()` in production. Use
it for socket and handshake deadlines, rate windows, retry/backoff, process
metrics, watchdogs, session elapsed time, and bounded worker waits. Its absolute
value is process-local: never persist it, log it as a calendar fact, or send it
as a meaningful protocol timestamp.

### UTC wall time

`server_wall_utc_t` is a Unix timestamp. Use it only for durable calendar facts
and operator/audit output such as account creation, last login, save metadata,
certificate validity, and heartbeat file timestamps. Do not use it for an
in-process elapsed-time decision. When a deadline must survive restart, persist
a UTC target and convert it with `server_wall_utc_remaining()`, which rejects
expired targets and clamps unreasonable remaining durations before creating a
new monotonic or simulation deadline.

## Arithmetic and testing rules

- Deadline addition and duration constructors saturate at `UINT64_MAX`, so the
  clock never wraps into the past. Expiry is inclusive: `now == deadline` is
  expired. Elapsed/difference helpers clamp a backward observation to zero.
- Tick conversion uses the configured simulation tick period. Duration-to-tick
  conversion rounds up so a deadline cannot expire early; tick-to-duration
  conversion reports overflow instead of truncating.
- Server startup installs the production clock from `reset_sleep()`. Only the
  main loop advances simulation time. A deliberate `/speed` change updates the
  conversion period without changing existing absolute tick deadlines.
- Unit tests may include `server/src/tests/server_clock_fake.h`, install a fake
  clock, and advance simulation and monotonic time independently without
  sleeping. Teardown must call `server_clock_fake_uninstall()`.

The first consumer migration stores pre-login deadlines as typed monotonic
values. Join-password rate windows are also monotonic, so wall-clock jumps can
neither reset nor extend those admission controls.

## Raw-clock audit and migration path

This audit covers remaining direct clock access under `server/src` as of
2026-08-06. Direct calls are retained only where listed; new call sites should
use the typed service.

| Location | Current purpose and classification | Migration |
| --- | --- | --- |
| `server/server_clock.c` | The sole production adapters for monotonic and UTC wall time. | Keep. |
| `server/time.c`, `server/main.c` (`GETTIMEOFDAY`) | Legacy loop cadence, spare pathfinding budget, and the legacy `seconds()` accessor. Cadence is a separate scheduling mechanism; `seconds()` is ambiguous. | Preserve cadence for now; later replace its wall-based measurement with monotonic scheduling in a dedicated behavior review and remove or retag `seconds()` callers. |
| `server/main.c` (`pticks` periodic work) | Gameplay/world updates are correctly simulation-based, but metaserver publication is process I/O and currently shares that schedule. | Keep gameplay periodic work on typed simulation deadlines; move metaserver retry/backoff and publication cadence to typed monotonic deadlines. |
| `server/main.c` (`datetime_monotonic_us`) | Game-loop performance metric. Correct domain, but bypasses the server type. | Replace with typed monotonic elapsed helpers. |
| `server/plugins.c`, `plugins/plugin_python/plugin_python.c` | Plugin performance measurements using `gettimeofday()`. Wrong domain because wall jumps can corrupt durations. | Replace with typed monotonic elapsed helpers. |
| `socket/assets.c` | Transfer budget windows and response performance metrics using toolkit monotonic values. Correct domain, untyped. | Migrate to typed monotonic deadlines and durations. |
| `socket/port_mapping.c` | Router-mapping renewal deadline using toolkit monotonic milliseconds and unchecked addition. Correct domain, overflow-prone. | Migrate to typed saturating monotonic deadlines. |
| `socket/metaserver.c` (`datetime_monotonic_ms`) | Rendezvous pacing. Correct domain, untyped. | Migrate to typed monotonic durations. |
| `socket/metaserver.c` (`GETTIMEOFDAY`) | Absolute deadline for a default realtime `pthread_cond_timedwait`. Wall jumps can alter the wait. | Initialize a monotonic condition variable where supported, with a portable fallback, then construct its deadline from a monotonic clock. |
| `socket/metaserver.c` (`time(NULL)`) | Human-readable last-success and last-failure operator timestamps. Correct wall domain. | Route through `server_wall_utc_now()` while retaining wall semantics. |
| `socket/server.c` (`time(NULL)`) | UTC heartbeat file consumed by an external container health check. Correct durable/operator wall timestamp. | Route timestamp acquisition through the wall accessor; use a separate monotonic deadline to rate-limit writes. |
| `server/account.c` (`datetime_getutc`) | Persisted account timestamps are correct wall facts; authentication-work refill is an in-process duration and is in the wrong domain. | Keep persisted facts as typed wall time and migrate work refill to monotonic time. |
| `server/swap.c` (`time(NULL)`) | Converts persisted map reset targets into remaining time during shutdown. Correct persistence boundary, but negative and unreasonable values are not clamped here. | Route through wall time and `server_wall_utc_remaining()` with a map-reset-specific maximum. |
| `types/player.c` gravestone timestamp | Authored calendar text. Correct wall domain. | Route through wall accessor before formatting. |
| `types/player.c` save throttling and session counters | In-process durations use wall time and can be distorted by clock changes. | Store monotonic session/save deadlines; persist only validated UTC facts that must survive restart. This is the session/autosave stage needed by issue #123. |

The gameplay migration order is: monster-dialog and encounter expiry; named
spawn cooldowns; target-dummy measurement windows; then action wind-ups and
other action timers. Each field should change from raw `pticks` or
`global_round_tag` arithmetic to a `server_tick_t` deadline together with its
producers, consumers, tests, and persistence behavior. Performance and network
call sites in the table can migrate independently because they do not change
simulation cadence.

Shared toolkit raw clocks are outside the server provider boundary. Logger
timestamps and certificate creation facts are wall time; toolkit QUIC/direct
socket deadlines are monotonic; `porting.c` owns platform clock/sleep fallbacks.
The fallback random seed in `toolkit/math.c` and signal-throttling wall timer in
`toolkit/signals.c` deserve separate security and monotonic-domain cleanup.
