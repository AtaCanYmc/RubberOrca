# Security Policy

## Supported Versions

| Version | Supported |
| :--- | :--- |
| `main` (v1.x) | Yes |

## Security Scope

RubberOrca emulates a wired USB HID keyboard and mouse controlled over BLE 5.0. Security reports of particular interest include:

- Unauthorized BLE access or bonding bypasses once pairing controls are introduced.
- Buffer overflows, integer overflows, or parser crashes in `PacketParser` and `CommandExecutor`.
- NVS corruption or unintended USB HID injection outside authorized commands.

## Reporting a Vulnerability

Do not open public GitHub issues for security vulnerabilities.

Report issues privately through one of these channels:

1. **GitHub Security Advisories:** [Report a vulnerability](https://github.com/AtaCanYmc/RubberOrca/security/advisories/new)
2. **Email:** `atacanymc@gmail.com`

Include the affected board target, firmware commit hash, reproduction steps, and any captured BLE or serial frames.

## Response SLA

- Initial acknowledgement within 48 hours.
- Triage and remediation timeline provided within 5 business days.
- Coordinated public disclosure after a patched release is available.
