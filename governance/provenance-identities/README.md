# Provenance identity registry

`registry.json` is the only public Atrinik provenance identity registry and
`schema-v1.json` is its canonical schema. The operating, privacy, custody,
attestation, reference, and migration rules are in
[`docs/PROVENANCE_IDENTITIES.md`](../../docs/PROVENANCE_IDENTITIES.md).

Validate the registry and any component references without network access:

```sh
./atrinik provenance validate --reference PATH
```

Never place real restricted identity evidence, aliases lacking explicit
publication authorization, encryption/HMAC keys, or private review notes here.
