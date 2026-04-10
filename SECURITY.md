# Security Policy

## Supported Versions

Security fixes are applied to the current development branch and to the latest release line when a release exists.

## Reporting a Vulnerability

If you believe you have found a security issue, please report it privately to the maintainers before opening a public issue.

Include:
- a short description of the issue
- affected version or commit
- reproduction steps
- any proof-of-concept material that helps us verify the problem

## Project-Specific Notes

- The built-in `rtl_tcp` backend uses cleartext TCP and does not authenticate clients.
- Treat `rtl_tcp` deployments as trusted-network workflows unless you add a separate transport protection layer.
- Do not include real credentials, API keys, or private capture material in public bug reports.
