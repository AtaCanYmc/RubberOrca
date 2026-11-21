# Contributing to RubberOrca

Contributions are welcome. Keep changes focused, preserve wire compatibility with RubberOtter v1 clients, and verify all build targets before opening a pull request.

## Development Workflow

1. Fork the repository and create a branch from `main` using a descriptive prefix: `feat/`, `fix/`, `docs/`, `refactor/`, or `test/`.
2. Keep protocol and command logic inside `include/` and `src/` free of ESP32-specific headers so `pio test -e native` stays runnable on any host.
3. Guard ESP32-only translation units with `#ifdef ARDUINO_ARCH_ESP32`.

## Commit Message Format

This repository follows [Conventional Commits](https://www.conventionalcommits.org/):

```text
<type>(<optional scope>): <short imperative summary>
```

Allowed types: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`, `ci`.

Examples:
- `feat(ble): negotiate MTU for faster notify throughput`
- `fix(parser): reset state machine on truncated ETX`
- `test(protocol): cover nested macro recursion limit`

## Local Verification

Run all three commands locally and confirm zero failures before submitting a pull request:

```bash
pio test -e native
pio run -e esp32s3_supermini
pio run -e esp32s3_n16r8
```

## Pull Request Checklist

- All native Unity tests pass.
- Both `esp32s3_supermini` and `esp32s3_n16r8` compile with zero warnings.
- Protocol or command changes include matching unit tests in `test/test_protocol/test_protocol.cpp`.
- Documentation in `README.md` reflects any new commands, flags, or hardware targets.
