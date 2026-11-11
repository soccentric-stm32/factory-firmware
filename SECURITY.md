# Security policy

## Supported versions

Security fixes are applied to the latest release on the default branch.

| Version | Supported |
| ------- | --------- |
| 0.1.x   | yes       |

## Reporting a vulnerability

Do not open a public issue for a security problem. Send a private report to
the maintainer listed in the repository profile, including:

- the affected version and board revision,
- a description of the impact,
- the steps to reproduce or a proof of concept,
- any suggested fix.

You will get an acknowledgement as soon as possible. Please allow time for a
fix and a coordinated release before publishing details.

## Scope

The firmware handles production identity and calibration data. Reports are
especially welcome for:

- writes outside the factory data window,
- bypasses of the file header, CRC or UID checks,
- ways to overwrite PRV1 or the BOOT metadata,
- watchdog or health-check bypasses that keep a bad slot active,
- parsing bugs in the HWLINK or factory command handlers.

The signing key is intentionally not part of this repository. Never commit a
production key; the `.gitignore` only allows the shared development key under
`keys/`.
