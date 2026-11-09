# Host tests for the factory command handler (no board needed).
CC ?= cc
CFLAGS ?= -Wall -Wextra -Werror -O1 -g

all: test libfactory_sim.so

test: test_factory
	./test_factory

test_factory: test_factory.c factory_sim.c ../../src/factory_proto.c
	$(CC) $(CFLAGS) -o $@ $^

libfactory_sim.so: factory_sim.c ../../src/factory_proto.c
	$(CC) $(CFLAGS) -shared -fPIC -o $@ $^

clean:
	rm -f test_factory libfactory_sim.so
