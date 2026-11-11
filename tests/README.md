# Host tests

The factory command handler is pure logic: every hardware access goes
through the `factory_ops_t` table in
[`src/protocol/factory_proto.h`](../src/protocol/factory_proto.h).
[`factory_sim.c`](host/factory_sim.c) implements that table against an in-memory
copy of the identity sector, so the tests need no board and no STM32 SDK.

```bash
make test        # from the repository root
make -C tests/host test
```

`make test` builds and runs [`test_factory.c`](host/test_factory.c), which covers
the factory command set, the status codes and the flash validation rules.
`libfactory_sim.so` is the same simulated handler built as a shared library
for the factory-station's Python tests.

The tests are built with the host compiler and `-Wall -Wextra -Werror`.
They do not use the FreeRTOS, HAL or flatcc headers, so they also run on a
plain Linux CI agent.
