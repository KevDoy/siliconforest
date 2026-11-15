# SPDX-License-Identifier: GPL-2.0-only OR MIT
# Host test of the table code. `make test`; needs only cc.
CC ?= cc
CFLAGS ?= -std=c11 -O1 -g -Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined

build/table_test: test/table_test.c src/pd_dart_table.c include/pd_dart_table.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude -o $@ test/table_test.c src/pd_dart_table.c

build/retire_test: test/retire_test.c src/pd_dart_table.c include/pd_dart_table.h include/pd_dart_retire.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude -o $@ test/retire_test.c src/pd_dart_table.c

test: build/table_test build/retire_test
	./build/table_test
	./build/retire_test

clean:
	rm -rf build

.PHONY: test clean
