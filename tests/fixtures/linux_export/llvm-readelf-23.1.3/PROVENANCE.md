# LLVM utility fixture provenance

The archiver, ranlib, nm, strip, objcopy and objdump records were captured from
private Debian portable SDK image
`sha256:2abebea3544caaa947920f6fc1fe80158352a1a0b3450a0331c1367676114c71`
on 2026-10-11 using SHA-256 of each installed executable and its `--version`
output in an isolated non-root CPU worker without network or host mounts. The pre-existing Debian driver
invocation remains unchanged. These records exercise strict parser compatibility;
they do not attest a published producer or replace the final immutable image and
metadata qualification.

`driver-evidence-cxx.json` adds the independently captured `cxx_compiler` record
from that same immutable SDK. Its `clang++ --version` output identifies the C++
driver configuration, and its executable SHA-256 was measured through the actual
`/opt/llvm-23.1.3/bin/clang++` selection. The original nine-tool
`driver-evidence.json` is retained unchanged as historical evidence and no longer
satisfies the current exact ten-role admission contract. All other records in the
new fixture are unchanged; these fixture records confer no producer authority.
