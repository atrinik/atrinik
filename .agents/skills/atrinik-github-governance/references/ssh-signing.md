# Optional SSH commit signing

This reference describes the optional SSH signing path for Atrinik contributors
and agents. It complements GitHub's [commit signature verification
guide](https://docs.github.com/en/authentication/managing-commit-signature-verification)
and keeps signing configuration outside the repository.

## Keep the host/container boundary explicit

- Signing configuration, the passphrase, and the passphrase-protected private
  key belong to the contributor host and personal account.
- Native host Git is the default place to create and sign commits. Use the
  Docker/devcontainer for builds, tests, and other container-owned work.
- An exceptional container commit may forward the host SSH agent through an
  explicitly supported `SSH_AUTH_SOCK` path and set only the required public
  signing metadata. Agent forwarding does not copy the private key.
- Never copy or bind-mount a private signing key, the private `.ssh` directory,
  or a personal Git configuration containing signing secrets into a source
  tree, image, container, generated state directory, or repository.
- SSH signing is optional. This procedure does not change repository rulesets or
  create a project-wide signed-commit policy.

## Choose or create a signing key

SSH signature support requires Git 2.34 or later. Check the host Git version
before changing configuration:

```powershell
git --version
```

A dedicated Ed25519 signing key keeps authentication and signing roles easier to
scope and revoke. Reusing an existing authentication key is also supported, but
the same public key must be registered with GitHub for signing as well as
authentication. Both choices are personal account configuration.

On Windows PowerShell, create a dedicated key when needed:

```powershell
ssh-keygen -t ed25519 -C "Atrinik Git signing" -f "$env:USERPROFILE\.ssh\id_ed25519_atrinik_signing"
```

Accept a passphrase at the prompt. Do not put a passphrase on the command line
or in a script, repository file, image, or generated state.

## Load the private key through the Windows OpenSSH agent

The agent holds the private key for the signing operation; the repository only
needs the public key path. In an elevated PowerShell session when the service
policy requires it:

```powershell
Get-Service ssh-agent | Set-Service -StartupType Manual
Start-Service ssh-agent
ssh-add "$env:USERPROFILE\.ssh\id_ed25519_atrinik_signing"
ssh-add -l
```

The final command should list the key fingerprint without printing private key
material. Keep the agent and private key on the Windows host.

## Register the public key with GitHub

Print or copy only the public-key file:

```powershell
Get-Content "$env:USERPROFILE\.ssh\id_ed25519_atrinik_signing.pub"
```

In GitHub, open **Settings -> SSH and GPG keys -> New SSH key**, select
**Signing Key**, and paste the `.pub` contents. If the key is also used for
repository authentication, register the same public key for that use as well.
Never paste the private key.

GitHub associates a signature with the account and commit author identity. Set
the commit email to an address verified on that GitHub account; a key comment
does not replace email verification:

```powershell
git config --global user.email "your-verified-address@example.com"
```

## Tell Git to use SSH signatures

Use the public key path when an SSH agent supplies the private key:

```powershell
git config --global gpg.format ssh
git config --global user.signingkey "$env:USERPROFILE\.ssh\id_ed25519_atrinik_signing.pub"
```

Signing can be enabled for all personal repositories:

```powershell
git config --global commit.gpgsign true
```

Alternatively, leave that default unset and opt in for an individual commit:

```powershell
git commit -S -m "docs: update contributor guidance"
```

These are user-level choices. Do not commit them as repository configuration.

## Verify the local signature and GitHub status separately

After creating a signed commit, inspect the local result:

```powershell
git show --show-signature --format=fuller -1 HEAD
git cat-file commit HEAD | Select-String '^gpgsig '
```

`git show --show-signature` reports the local verification result when the
host has the relevant verifier configuration. The `gpgsig` header check only
confirms that the commit object contains a signature block; neither check alone
proves GitHub will display **Verified**.

Push the commit, open the commit or pull request's **Commits** view on GitHub,
and inspect the badge beside the commit. **Verified** means GitHub matched and
verified the signature using the registered public signing key and the
account's verified author email. If the badge is absent or says **Unverified**,
check Git version, agent state, signing-key registration, and the author email.

Cryptographic signing is different from a sign-off trailer. `git commit -s`
adds a `Signed-off-by` line to the message; it does not create an SSH
signature or make GitHub show **Verified**. `git commit -S` requests the
cryptographic signature.
