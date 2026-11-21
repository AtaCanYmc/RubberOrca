## Description

<!-- Provide a concise technical summary of the changes introduced in this PR. -->

## Changes

<!-- List specific files, functions, or configurations modified. -->
-

## Type of Change

- [ ] `feat`: New firmware capability, command, or board target
- [ ] `fix`: Bug fix or protocol/hardware correction
- [ ] `docs`: Documentation updates or additions
- [ ] `refactor`: Structural change without behavioral impact
- [ ] `test`: New or updated unit tests
- [ ] `chore`: Tooling, CI, or dependency updates

## Verification

```bash
pio test -e native
pio run -e esp32s3_supermini
pio run -e esp32s3_n16r8
```

## Checklist

- [ ] Commits follow [Conventional Commits](https://www.conventionalcommits.org/).
- [ ] Documentation in `README.md` reflects any new commands or board targets.
- [ ] All native unit tests and both ESP32-S3 firmware builds pass locally.
