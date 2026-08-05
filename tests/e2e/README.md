# Direct-network end-to-end tests

This suite exercises Atrinik's direct UDP/QUIC stack on isolated loopback
sockets. It creates all identities in a temporary directory and has bounded
process and network deadlines, so it never reads or changes `server/data/`.

From the repository root, configure the normal Linux build and run:

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug --target network-e2e
```

The aggregate target covers:

- QUIC identity creation, mode `0600`, persistence, and corrupt-file failure;
- a real OpenSSL QUIC handshake, ALPN, certificate pinning, shared connection
  identity, bidirectional application data, and wrong-pin rejection;
- rapid client and server peer-close detection with a one-second upper bound;
- STUN binding discovery through a local RFC 5389 responder, including a
  malformed transaction response;
- bounded bilateral UDP punch probes and observed source endpoints; and
- deterministic PCP/NAT-PMP-first selection, UPnP fallback, renewal dispatch,
  and cleanup through fake port-mapping backends.

Run one or more focused scenarios directly after building the driver:

```sh
python3 tests/e2e/run.py --scenario quic --scenario stun --verbose
```

The port-mapping scenario validates Atrinik's orchestration and lifecycle, not
a particular home router. Real PCP, NAT-PMP, and UPnP interoperability requires
an opt-in lab or network-namespace suite with an emulated gateway; it is not a
safe or deterministic default CI dependency. Metaserver directory and
rendezvous behavior is covered in the local Worker runtime by
`npm --prefix metaserver/worker test`.
