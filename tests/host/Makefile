# Host tests for the factory command handler (no board needed).
#
# The command handler is pure logic (src/protocol/factory_proto.c): every
# hardware access goes through factory_ops_t, so the same code runs on the
# board and against the simulated flash in factory_sim.c.

# mingw-w64 ships gcc but no cc; use it when there is no cc.
ifeq ($(origin CC),default)
CC     := $(if $(shell command -v cc 2>/dev/null),cc,gcc)
endif
CFLAGS ?= -Wall -Wextra -Werror -O1 -g
INCLUDES := -I../../src/protocol -I../../src
SRC      := ../../src/protocol/factory_proto.c ../../src/protocol/factory_fb.c

all: test libfactory_sim.so

test: test_factory
	./test_factory

test_factory: test_factory.c factory_sim.c $(SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

libfactory_sim.so: factory_sim.c $(SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -shared -fPIC -o $@ $^

clean:
	rm -f test_factory libfactory_sim.so

.PHONY: all test clean
