# Contributing

Thanks for taking the time to improve the factory firmware. This project is
small and hardware-oriented; the rules below keep the image reproducible and
the boot contract intact.

## Before you start

- Read [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) to see where a change
  belongs in the source layers.
- Read [docs/FACTORY_SPEC.md](docs/FACTORY_SPEC.md) before touching the
  command set, file formats or flash addresses. The factory station depends
  on that contract.
- For a change that affects BOOT, update
  [`src/platform/boot_contract.h`](src/platform/boot_contract.h) and the
  "Boot contract" values in `Jenkinsfile` together.

## Workflow

1. Fork the repository and create a topic branch:

   ```bash
   git switch -c feature/short-description
   ```

2. Make the change in the smallest sensible layer. Keep the dependency
   direction described in the architecture document.
3. Build and test:

   ```bash
   make test                       # host tests, no board needed
   make build                      # cross build in the stm32-dev container
   make lint                       # clang-tidy
   ```

4. Commit with a focused message. The body should say what changed and why.
5. Open a pull request. Describe the behaviour change and the tests you ran.

## C style

The project uses the formatting in [`.clang-format`](.clang-format) and the
analysis rules in [`.clang-tidy`](.clang-tidy).

- Allman braces, 4-space indent, 90-column limit.
- `snake_case` for functions, `UPPER_SNAKE_CASE` for macros and constants.
- Static functions and file-local state are preferred over globals.
- No dynamic allocation after start-up. Use the FreeRTOS heap or a static
  buffer.
- Keep the generated `src/generated/` headers out of manual edits. Change
  `protocol/hwlink.fbs` and regenerate them instead.

Run `make format` before committing if your editor does not apply the
configuration automatically. The target skips `src/generated/`.

## Tests

Host tests live in [`tests/host`](tests/README.md). Add a case to
`test_factory.c` for every command, status code or validation rule you add.
A test must pass with:

```bash
make test
```

If you change the flatcc schema, install `flatcc` 0.6.1 and regenerate:

```bash
flatcc -a -o src/generated protocol/hwlink.fbs
```

Commit both the schema and the generated headers.

## Pull request checklist

- [ ] Behaviour is covered by a host test, or the reason it cannot be is
      explained.
- [ ] `make test` and `make build` pass.
- [ ] `make lint` has no new findings.
- [ ] No new compiler warnings (`-Wall -Wextra -Werror` is on).
- [ ] Documentation is updated when the user-facing contract changes.
- [ ] The image still fits the flash and RAM budgets.

## Reporting a problem

Open an issue with the board revision, the firmware version
(`INFO` reply or the debug banner), the steps to reproduce and the relevant
debug log lines. Do not paste private keys or production identity data.
