# Classic tools ownership and migration

[`governance/classic-tools.json`](../governance/classic-tools.json) inventories
every retained top-level entry point in `atrinik/tools`. Its GPL audit baseline is
`7777cf9f9ab6deb58de8a481dfccd6b05d86e3e1` and tree
`daeb2eb5771d3f90ecf70ccfa2d9e1e4d768f6e4`; it is not the transitioned
tree. The reviewed transition target is
`6caf3d4b1f1baf7034269be406b5c12437f5ccda` (tree
`7de8e8f721cb8cf68a7ac0d128971206f41a8695`): MIT by default with a
GPL-2.0-or-later `map-checker-qt/` exception. Complete-checkout metadata uses
`LicenseRef-Atrinik-Tools-Mixed` until the checker is removed.
The repository remains an explicit optional member of the classic cohort only;
plain `./atrinik init` and the `default` profile neither select it nor depend on
it for a build or runtime.

| Retained command/workflow | Replacement or removal | Owner and gate |
| --- | --- | --- |
| `python3 map-checker-qt/map-checker.py --cli` | `atrinik-content check`; retain the Qt checker until diagnostic parity | `atrinik/content-toolkit#8`, M4 |
| `split_symbols.sh` and `stacktrace.py` | Keep only for disposable classic release diagnostics; replacement owners use native Go/Rust symbol tooling | `atrinik/classic#4`, M6 |

The JSON record is authoritative for paths, consumers, inputs, outputs, current
usage, license method, security controls, and exact verification. Retired
utilities and their migration ownership entries are absent from the live
inventory; their Git history and published releases remain unchanged.
Replacement owners may use documented observable behavior as compatibility
evidence. Existing fixtures may be copied only when
their license is compatible with the intended destination or an exact grant
record permits it; otherwise create synthetic or black-box fixtures without
translating GPL implementation. Exact, independently separable implementation
proven to fall within an applicable
approved historical grant may be inspected as source reference, copied,
migrated or ported, translated or adapted, or MIT-relicensed when the full root
process in [`REPLACEMENT_FOUNDATIONS.md`](REPLACEMENT_FOUNDATIONS.md) passes.
Later or uncovered implementation remains excluded.

The final tools support gate is removal of `map-checker-qt/` after replacement
parity and safety fixtures pass. That follow-up removes the scoped GPL license,
root exception wording, and mixed-license metadata, then changes the complete
checkout/component license to MIT. Until then, tools remain optional classic
operator utilities—not a hidden dependency of classic services and never a
dependency of a production replacement build or runtime.

## Classic repository disposition

Operational classic development is owned only by `atrinik/classic`, with
logical sources at `client/`, `server/`, `editor/`, `libatrinik/`, and
`protocol/`. `components.json`, profiles, the supply-chain inventory, and
GitHub desired state use that checkout. References to former `legacy-*`
repositories are limited to checked local-worktree migration, archived source
history, and immutable historical release inputs.

The ported classic client PR was preserved and explicitly closed after green CI
and review because its broad feature architecture exceeded the classic
critical-maintenance boundary and duplicated the released MIT client
foundation. It was not silently merged. The former repositories remain
archived read-only; the wrapper migration retains recoverable local work and
the supply-chain catalog resolves active module paths through `atrinik/classic`.
